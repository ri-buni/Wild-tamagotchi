// =====================================================================
// Tamagotchi Friends — Rabbit & Bunny
// Hardware: ESP32-S3-DEVKITC-1-N8R2 + 2.42" OLED (SSD1309 128x64 I2C) + 4 buttons
// Pin map taken from inzhugotchi.kicad_pcb (your schematic).
// =====================================================================
//
// WIRING (matches your PCB)
//
//   OLED (I2C, connector J4)
//     VCC -> 3V3 (or 5V depending on module)
//     GND -> GND
//     SDA -> GPIO  8
//     SCL -> GPIO  9
//
//   Buttons (each from GPIO to GND; internal pull-ups enabled)
//     SW1  LEFT          -> GPIO 15
//     SW2  RIGHT         -> GPIO 16
//     SW3  ACTION        -> GPIO 17
//     SW4  SWITCH PET    -> GPIO 18   (double-press = display on/off)
//
// CONTROLS
//
//   | Context     | LEFT (1)      | RIGHT (2)     | ACTION (3)    | SWITCH (4)            |
//   |-------------|---------------|---------------|---------------|-----------------------|
//   | Normal      | walk left     | walk right    | open menu     | 1x: switch Rabbit/Bunny|
//   | Menu open   | selection up  | selection down| confirm       | 1x: cancel menu       |
//   | Any         |               |               |               | 2x fast: display on/off|
//
//   The world is a row of 5 locations:
//     [R.House] - [Garden] - [Meadow] - [Pond] - [B.House]
//   LEFT/RIGHT walk the *active* pet between them. The screen always
//   follows the active pet. Walking to your friend's house = visiting.
//   When both pets are in the same place they chat (dialog alternates).
//
//   Location bonuses:  Feed at Garden (+45), Play at Meadow (+45),
//   Clean at Pond (+60), Sleep at own home (+40, elsewhere +25).
//
// SETUP
//   1. Install U8g2 library (Library Manager -> "U8g2 by oliver")
//   2. Board: "ESP32S3 Dev Module"
//   3. Edit WIFI_SSID / WIFI_PASS below
//   4. Upload
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <time.h>

#include "dialogs.h"
#include "graphics.h"  // defines Location enum + Display typedef, uses extern u8g2

// ---------- USER SETTINGS ----------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Almaty (UTC+5), no DST
const char* TZ_INFO    = "ALMT-5";
const char* NTP_SERVER = "pool.ntp.org";

// Wait this long (ms) for WiFi before giving up and running offline
const uint32_t WIFI_TIMEOUT_MS = 8000;

// ---------- PIN MAP (from inzhugotchi schematic) ----------
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9

#define PIN_BTN_LEFT   15   // SW1
#define PIN_BTN_RIGHT  16   // SW2
#define PIN_BTN_ACT    17   // SW3
#define PIN_BTN_SWAP   18   // SW4 (double-press = display power)

// ---------- DISPLAY ----------
// 2.42" mono OLED, I2C. If the screen stays blank but the board boots,
// edit the `Display` typedef at the top of graphics.h (NONAME0 -> NONAME2).
Display u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- PET DATA ----------
enum PetState { S_IDLE = 0, S_EAT = 1, S_PLAY = 2, S_SLEEP = 3, S_SAD = 4 };

struct Pet {
  const char* name;
  uint8_t hunger;     // 0..100  (100 = stuffed,  0 = starving)
  uint8_t happy;      // 0..100
  uint8_t energy;     // 0..100
  uint8_t clean;      // 0..100
  uint8_t stage;      // 0..4  (baby..elder)
  uint32_t stageMs;   // ms accumulated toward next stage
  PetState state;     // current animation state
  uint32_t stateEndMs;// when to revert to IDLE
  uint32_t lastDialogMs;
  const char* dialog;
  Location loc;       // where the pet currently is
  Location home;      // its own house
};

Pet rabbit = { "Rabbit", 80, 80, 80, 80, 0, 0, S_IDLE, 0, 0, "...",       LOC_RHOME, LOC_RHOME };
Pet bunny  = { "Bunny",  80, 80, 80, 80, 0, 0, S_IDLE, 0, 0, "HI HI HI!", LOC_BHOME, LOC_BHOME };

// ---------- GAME STATE ----------
Pet*      active     = &rabbit;   // which pet the buttons control / camera follows
bool      menuOpen   = false;
int       menuSel    = 0;
bool      displayOn  = true;

// Random meet-up event: both pets head to the meadow together
bool      meetEvent       = false;
uint32_t  meetEndMs       = 0;
uint32_t  nextMeetCheckMs = 0;

// Animation frame counter
uint32_t  frame = 0;
uint32_t  lastFrameMs = 0;

// Stat decay tick
uint32_t  lastStatTickMs = 0;
const uint32_t STAT_TICK_MS = 8000;  // every 8s -> stats drop a bit

// Evolution timing — 6 minutes per stage by default (4 stages = 24 min to elder)
const uint32_t STAGE_DURATION_MS = 6UL * 60UL * 1000UL;

// Time-of-day cache
int currentHour = 12;

// =====================================================================
// Button debounce
// =====================================================================
struct Button {
  uint8_t pin;
  bool    lastReading;
  bool    state;
  uint32_t lastChangeMs;
  bool    pressedEdge;  // true for one frame on press
};

Button btnLeft  = { PIN_BTN_LEFT,  HIGH, HIGH, 0, false };
Button btnRight = { PIN_BTN_RIGHT, HIGH, HIGH, 0, false };
Button btnAct   = { PIN_BTN_ACT,   HIGH, HIGH, 0, false };
Button btnSwap  = { PIN_BTN_SWAP,  HIGH, HIGH, 0, false };

void updateButton(Button& b) {
  b.pressedEdge = false;
  bool r = digitalRead(b.pin);
  if (r != b.lastReading) {
    b.lastReading = r;
    b.lastChangeMs = millis();
  }
  if (millis() - b.lastChangeMs > 30) {  // 30ms debounce
    if (r != b.state) {
      b.state = r;
      if (b.state == LOW) b.pressedEdge = true;  // falling edge = press
    }
  }
}

// ----- single vs double press resolver for the SWITCH button -----
// A press is held for up to DOUBLE_PRESS_MS waiting for a second press.
// Second press inside the window  -> double (display on/off).
// Window expires with one press   -> single (switch pet / cancel menu).
const uint32_t DOUBLE_PRESS_MS = 350;
uint32_t swapFirstPressMs = 0;
bool     swapWaiting      = false;

void resolveSwapPress(bool& single, bool& dbl) {
  single = false; dbl = false;
  if (btnSwap.pressedEdge) {
    if (swapWaiting && millis() - swapFirstPressMs <= DOUBLE_PRESS_MS) {
      dbl = true;
      swapWaiting = false;
    } else {
      swapWaiting = true;
      swapFirstPressMs = millis();
    }
  } else if (swapWaiting && millis() - swapFirstPressMs > DOUBLE_PRESS_MS) {
    single = true;
    swapWaiting = false;
  }
}

// =====================================================================
// WiFi + NTP
// =====================================================================
void connectWifiAndTime() {
  Serial.printf("Connecting to %s ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
    configTzTime(TZ_INFO, NTP_SERVER);
    struct tm tm;
    uint32_t t0 = millis();
    while (!getLocalTime(&tm, 100) && millis() - t0 < 3000) { delay(50); }
    if (getLocalTime(&tm)) {
      Serial.printf("Time synced: %02d:%02d\n", tm.tm_hour, tm.tm_min);
    } else {
      Serial.println("Time sync failed; running offline.");
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    Serial.println("WiFi failed; running offline (clock starts at noon).");
  }
}

void updateHour() {
  struct tm tm;
  if (getLocalTime(&tm, 5)) {
    currentHour = tm.tm_hour;
  } else {
    currentHour = (12 + (int)(millis() / 3600000UL)) % 24;
  }
}

void getTimeString(char* buf, size_t n) {
  struct tm tm;
  if (getLocalTime(&tm, 5)) {
    snprintf(buf, n, "%02d:%02d", tm.tm_hour, tm.tm_min);
  } else {
    snprintf(buf, n, "--:--");
  }
}

// =====================================================================
// Small helpers
// =====================================================================
bool isRabbitP(const Pet& p) { return &p == &rabbit; }
Pet& otherPet(const Pet& p)  { return isRabbitP(p) ? bunny : rabbit; }

void say(Pet& p, const char** lines, int count) {
  p.dialog = lines[random(count)];
  p.lastDialogMs = millis();
}

void clampStat(uint8_t& v, int delta) {
  int nv = (int)v + delta;
  if (nv < 0) nv = 0;
  if (nv > 100) nv = 100;
  v = nv;
}

// =====================================================================
// Stats / evolution
// =====================================================================
void decayStats(Pet& p) {
  bool night = isNight(currentHour);
  int dHunger = night ? -1 : -2;
  int dHappy  = night ?  0 : -1;
  int dEnergy = night ? +3 : -1;   // recovers at night
  int dClean  = -1;

  clampStat(p.hunger, dHunger);
  clampStat(p.happy,  dHappy);
  clampStat(p.energy, dEnergy);
  clampStat(p.clean,  dClean);

  if (p.state == S_IDLE) {
    if (p.energy < 20)      p.state = S_SLEEP;
    else if (p.hunger < 25 || p.happy < 25 || p.clean < 25) p.state = S_SAD;
  }
  if (p.state == S_SLEEP && p.energy > 70) p.state = S_IDLE;
  if (p.state == S_SAD && p.hunger >= 25 && p.happy >= 25 && p.clean >= 25) p.state = S_IDLE;
}

void advanceEvolution(Pet& p, uint32_t dt) {
  if (p.stage >= 4) return;
  p.stageMs += dt;
  if (p.stageMs >= STAGE_DURATION_MS) {
    p.stageMs = 0;
    p.stage++;
    p.state = S_PLAY;
    p.stateEndMs = millis() + 2500;
    say(p, isRabbitP(p) ? rabbitEvolve : bunnyEvolve, 3);
  }
}

const char* stageName(int s) {
  switch (s) {
    case 0: return "baby";
    case 1: return "child";
    case 2: return "teen";
    case 3: return "adult";
    default: return "elder";
  }
}

// =====================================================================
// Movement & locations
// =====================================================================
void onArrive(Pet& p) {
  bool isR = isRabbitP(p);

  switch (p.loc) {
    case LOC_RHOME:  say(p, isR ? rabbitHome   : bunnyVisit,  3); break; // bunny at rabbit's = visiting
    case LOC_GARDEN: say(p, isR ? rabbitGarden : bunnyGarden, 3); break;
    case LOC_MEADOW: say(p, isR ? rabbitMeadow : bunnyMeadow, 3); break;
    case LOC_POND:   say(p, isR ? rabbitPond   : bunnyPond,   3); break;
    case LOC_BHOME:  say(p, isR ? rabbitVisit  : bunnyHome,   3); break; // rabbit at bunny's = visiting
    default: break;
  }

  // If the friend is already here, the friend greets back
  Pet& o = otherPet(p);
  if (o.loc == p.loc) {
    say(o, isRabbitP(o) ? rabbitMeet : bunnyMeet, 3);
    clampStat(p.happy, +5);   // company is nice
    clampStat(o.happy, +5);
  }
}

void movePet(int dir) {
  int nl = (int)active->loc + dir;
  if (nl < 0 || nl >= NUM_LOCS) return;  // edge of the world

  // Player movement interrupts a random meet-up: the other pet heads home
  if (meetEvent) {
    meetEvent = false;
    Pet& o = otherPet(*active);
    o.loc = o.home;
    say(o, isRabbitP(o) ? rabbitHome : bunnyHome, 3);
  }

  // walking wakes a napping pet
  if (active->state == S_SLEEP) active->state = S_IDLE;

  active->loc = (Location)nl;
  onArrive(*active);
}

void switchPet() {
  active = (active == &rabbit) ? &bunny : &rabbit;
  active->lastDialogMs = 0;   // greet on the next dialog refresh
}

// =====================================================================
// Actions (from menu) — with location bonuses
// =====================================================================
void doFeed(Pet& p) {
  bool garden = (p.loc == LOC_GARDEN);
  clampStat(p.hunger, garden ? +45 : +30);
  clampStat(p.clean,  -5);
  p.state = S_EAT;
  p.stateEndMs = millis() + 2000;
  bool isR = isRabbitP(p);
  if (garden) say(p, isR ? rabbitFeedGarden : bunnyFeedGarden, 3);
  else        say(p, isR ? rabbitEat        : bunnyEat,        3);
}

void doPlay(Pet& p) {
  bool meadow = (p.loc == LOC_MEADOW);
  clampStat(p.happy,  meadow ? +45 : +30);
  clampStat(p.energy, -15);
  clampStat(p.hunger, -5);
  p.state = S_PLAY;
  p.stateEndMs = millis() + 2500;
  bool isR = isRabbitP(p);
  if (meadow) say(p, isR ? rabbitPlayMeadow : bunnyPlayMeadow, 3);
  else        say(p, isR ? rabbitPlay       : bunnyPlay,       3);
}

void doClean(Pet& p) {
  bool pond = (p.loc == LOC_POND);
  clampStat(p.clean, pond ? +60 : +40);
  p.state = S_IDLE;
  bool isR = isRabbitP(p);
  if (pond) say(p, isR ? rabbitCleanPond : bunnyCleanPond, 3);
  else      say(p, isR ? rabbitClean     : bunnyClean,     3);
}

void doSleep(Pet& p) {
  bool atHome = (p.loc == p.home);
  p.state = S_SLEEP;
  p.stateEndMs = millis() + 4000;
  clampStat(p.energy, atHome ? +40 : +25);
  bool isR = isRabbitP(p);
  if (atHome) say(p, isR ? rabbitSleep     : bunnySleep,     3);
  else        say(p, isR ? rabbitSleepAway : bunnySleepAway, 3);
}

void runAction(int idx) {
  Pet& p = *active;
  switch (idx) {
    case 0: doFeed(p);  break;
    case 1: doPlay(p);  break;
    case 2: doClean(p); break;
    case 3: doSleep(p); break;
    case 4: /* Back */  break;
  }
}

// =====================================================================
// Random meadow meet-ups
// =====================================================================
void maybeTriggerMeet() {
  if (meetEvent || menuOpen) return;
  if (millis() < nextMeetCheckMs) return;
  nextMeetCheckMs = millis() + 60000;  // re-check every minute

  if (isNight(currentHour)) return;          // no trips at night
  if (rabbit.loc == bunny.loc) return;       // already together

  if (random(100) < 25) {                    // ~25% chance per check
    meetEvent = true;
    meetEndMs = millis() + 15000;
    rabbit.loc = LOC_MEADOW;
    bunny.loc  = LOC_MEADOW;
    if (rabbit.state == S_SLEEP) rabbit.state = S_IDLE;
    if (bunny.state  == S_SLEEP) bunny.state  = S_IDLE;
    say(rabbit, rabbitMeet, 3);
    say(bunny,  bunnyMeet,  3);
  }
}

void endMeetIfExpired(uint32_t now) {
  if (meetEvent && now > meetEndMs) {
    meetEvent = false;
    rabbit.loc = rabbit.home;
    bunny.loc  = bunny.home;
    say(rabbit, rabbitHome, 3);
    say(bunny,  bunnyHome,  3);
  }
}

// =====================================================================
// Need-driven complaints + idle chatter
// =====================================================================
uint32_t rabbitLastComplaintMs = 0;
uint32_t bunnyLastComplaintMs  = 0;

void maybeComplain(Pet& p, uint32_t& lastMs) {
  if (millis() - lastMs < 18000) return;       // 18s cooldown per pet
  if (p.state != S_IDLE && p.state != S_SAD) return;

  int worst = 101;
  int which = -1;   // 0=hunger 1=energy 2=clean 3=happy
  if (p.hunger < 30 && p.hunger < worst) { worst = p.hunger; which = 0; }
  if (p.energy < 25 && p.energy < worst) { worst = p.energy; which = 1; }
  if (p.clean  < 25 && p.clean  < worst) { worst = p.clean;  which = 2; }
  if (p.happy  < 30 && p.happy  < worst) { worst = p.happy;  which = 3; }
  if (which < 0) return;

  bool isR = isRabbitP(p);
  switch (which) {
    case 0: say(p, isR ? rabbitHungry : bunnyHungry, 3); break;
    case 1: say(p, isR ? rabbitTired  : bunnyTired,  3); break;
    case 2: say(p, isR ? rabbitDirty  : bunnyDirty,  3); break;
    case 3: say(p, isR ? rabbitBored  : bunnyBored,  3); break;
  }
  lastMs = millis();
}

void refreshIdleDialog(Pet& p) {
  if (millis() - p.lastDialogMs < 6000) return;
  if (p.state != S_IDLE && p.state != S_SAD) return;
  bool night = isNight(currentHour);
  if (isRabbitP(p)) say(p, night ? rabbitNight : rabbitIdle, night ? rabbitNightCount : rabbitIdleCount);
  else              say(p, night ? bunnyNight  : bunnyIdle,  night ? bunnyNightCount  : bunnyIdleCount);
}

// =====================================================================
// Drawing top-level
// =====================================================================
void drawScene() {
  u8g2.clearBuffer();
  char tbuf[8];
  getTimeString(tbuf, sizeof(tbuf));

  Pet& p = *active;
  Location vloc = p.loc;   // camera follows the active pet

  // Background for wherever the active pet is
  drawLocation(vloc, currentHour, frame);

  // Top status bar: "Rabbit(teen)" left, location small in middle, time right
  char label[24];
  snprintf(label, sizeof(label), "%s(%s)", p.name, stageName(p.stage));
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 8, label);
  u8g2.drawStr(108, 8, tbuf);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(72, 8, LOC_NAMES[vloc]);
  u8g2.drawHLine(0, 10, 128);

  // Stats panel + nav arrows (hidden while the menu overlaps)
  if (!menuOpen) {
    drawStatsPanel(p.hunger, p.happy, p.energy, p.clean);
    drawNavHints(vloc);
  }

  // Pets
  bool together = (rabbit.loc == bunny.loc);
  if (together) {
    drawRabbit(45, 40, frame, rabbit.stage, rabbit.state);
    drawBunny (83, 40, frame, bunny.stage,  bunny.state);
    drawActiveMarker((active == &rabbit) ? 45 : 83, 16);
  } else {
    if (active == &rabbit) drawRabbit(58, 40, frame, rabbit.stage, rabbit.state);
    else                   drawBunny (58, 40, frame, bunny.stage,  bunny.state);
  }

  // Dialog: alternate between the two friends every ~3s when together
  const char* line = p.dialog;
  if (together) {
    line = (((millis() / 3000) & 1) == 0) ? rabbit.dialog : bunny.dialog;
  }
  drawDialogBox(line);

  if (menuOpen) drawActionMenu(menuSel);

  u8g2.sendBuffer();
}

// =====================================================================
// Setup / Loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(PIN_BTN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_ACT,   INPUT_PULLUP);
  pinMode(PIN_BTN_SWAP,  INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);  // 400kHz — fast mode

  u8g2.begin();
  u8g2.setContrast(180);

  // splash
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(8, 26, "Rabbit & Bunny");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(28, 42, "connecting wifi...");
  u8g2.sendBuffer();

  randomSeed(esp_random());

  connectWifiAndTime();
  updateHour();

  lastFrameMs     = millis();
  lastStatTickMs  = millis();
  nextMeetCheckMs = millis() + 30000;
}

void loop() {
  // ----- input -----
  updateButton(btnLeft);
  updateButton(btnRight);
  updateButton(btnAct);
  updateButton(btnSwap);

  bool swapSingle, swapDouble;
  resolveSwapPress(swapSingle, swapDouble);

  // Double-press SWITCH: display power, works whether the screen is on or off
  if (swapDouble) {
    displayOn = !displayOn;
    u8g2.setPowerSave(displayOn ? 0 : 1);
    if (!displayOn) menuOpen = false;  // tidy up: close menu when sleeping
  }

  if (displayOn) {
    if (swapSingle) {
      if (menuOpen) menuOpen = false;  // SWITCH also backs out of the menu
      else          switchPet();
    }

    if (menuOpen) {
      if (btnLeft.pressedEdge)  menuSel = (menuSel + NUM_ACTIONS - 1) % NUM_ACTIONS;
      if (btnRight.pressedEdge) menuSel = (menuSel + 1) % NUM_ACTIONS;
      if (btnAct.pressedEdge) {
        if (menuSel != ACTION_BACK) runAction(menuSel);
        menuOpen = false;
      }
    } else {
      if (btnLeft.pressedEdge)  movePet(-1);
      if (btnRight.pressedEdge) movePet(+1);
      if (btnAct.pressedEdge) { menuOpen = true; menuSel = 0; }
    }
  }

  // ----- timed updates -----
  uint32_t now = millis();
  uint32_t dt  = now - lastFrameMs;

  // Animation frame every ~250ms
  if (dt >= 250) {
    frame++;
    lastFrameMs = now;
    updateHour();

    refreshIdleDialog(rabbit);
    refreshIdleDialog(bunny);

    // Expire transient states
    if (rabbit.stateEndMs && now > rabbit.stateEndMs) { rabbit.state = S_IDLE; rabbit.stateEndMs = 0; }
    if (bunny.stateEndMs  && now > bunny.stateEndMs)  { bunny.state  = S_IDLE; bunny.stateEndMs  = 0; }

    endMeetIfExpired(now);
    maybeTriggerMeet();
  }

  // Stat decay tick
  if (now - lastStatTickMs >= STAT_TICK_MS) {
    uint32_t delta = now - lastStatTickMs;
    lastStatTickMs = now;
    decayStats(rabbit);
    decayStats(bunny);
    advanceEvolution(rabbit, delta);
    advanceEvolution(bunny,  delta);
    maybeComplain(rabbit, rabbitLastComplaintMs);
    maybeComplain(bunny,  bunnyLastComplaintMs);
  }

  // ----- draw -----
  if (displayOn) drawScene();

  delay(15);  // light idle
}
