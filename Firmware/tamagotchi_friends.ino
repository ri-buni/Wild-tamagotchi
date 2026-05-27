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
//     SW1  Button1 (action/cycle)   -> GPIO 15
//     SW2  Button2 (confirm/dialog) -> GPIO 16
//     SW3  Button3 (switch view)    -> GPIO 17
//     SW4  ON/OFF  (display toggle) -> GPIO 18
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
#include "graphics.h"  // uses extern u8g2

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

#define PIN_BTN_A      15   // SW1 - Button1
#define PIN_BTN_B      16   // SW2 - Button2
#define PIN_BTN_C      17   // SW3 - Button3
#define PIN_BTN_D      18   // SW4 - ON/OFF (display toggle, software-readable)

// ---------- DISPLAY ----------
// 2.42" mono OLED, I2C. If the screen stays blank but the board boots,
// try the NONAME2 variant (different RAM layout on some panels).
U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ---------- PET DATA ----------
enum PetState  { S_IDLE = 0, S_EAT = 1, S_PLAY = 2, S_SLEEP = 3, S_SAD = 4 };
enum SceneView { V_RABBIT = 0, V_BUNNY = 1, V_OUTSIDE = 2 };

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
};

Pet rabbit = { "Rabbit", 80, 80, 80, 80, 0, 0, S_IDLE, 0, 0, "..." };
Pet bunny  = { "Bunny",  80, 80, 80, 80, 0, 0, S_IDLE, 0, 0, "HI HI HI!" };

// ---------- GAME STATE ----------
SceneView view        = V_RABBIT;
bool      menuOpen    = false;
int       menuSel     = 0;
bool      displayOn   = true;

// Outside event: when both pets are out together
bool      outsideEvent      = false;
uint32_t  outsideEndMs      = 0;
uint32_t  nextOutsideCheckMs = 0;

// Visit event: one pet visiting the other's house
bool      visiting          = false;    // is someone currently visiting?
bool      rabbitVisitingBunny = false;  // direction
uint32_t  visitEndMs        = 0;

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

Button btnA = { PIN_BTN_A, HIGH, HIGH, 0, false };
Button btnB = { PIN_BTN_B, HIGH, HIGH, 0, false };
Button btnC = { PIN_BTN_C, HIGH, HIGH, 0, false };
Button btnD = { PIN_BTN_D, HIGH, HIGH, 0, false };

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
    // wait briefly for time sync
    struct tm tm;
    uint32_t t0 = millis();
    while (!getLocalTime(&tm, 100) && millis() - t0 < 3000) { delay(50); }
    if (getLocalTime(&tm)) {
      Serial.printf("Time synced: %02d:%02d\n", tm.tm_hour, tm.tm_min);
    } else {
      Serial.println("Time sync failed; running offline.");
    }
    // disable radio after sync to save power & cut RF noise
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
    // fallback: rough drift based on millis since boot, start at noon
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
// Stats / evolution / state transitions
// =====================================================================
void clampStat(uint8_t& v, int delta) {
  int nv = (int)v + delta;
  if (nv < 0) nv = 0;
  if (nv > 100) nv = 100;
  v = nv;
}

void decayStats(Pet& p) {
  // night = slower decay (they sleep)
  bool night = isNight(currentHour);
  int dHunger = night ? -1 : -2;
  int dHappy  = night ?  0 : -1;
  int dEnergy = night ? +3 : -1;   // recovers at night
  int dClean  = -1;

  clampStat(p.hunger, dHunger);
  clampStat(p.happy,  dHappy);
  clampStat(p.energy, dEnergy);
  clampStat(p.clean,  dClean);

  // If stats are bad, become sad / sleepy
  if (p.state == S_IDLE) {
    if (p.energy < 20)      p.state = S_SLEEP;
    else if (p.hunger < 25 || p.happy < 25 || p.clean < 25) p.state = S_SAD;
  }
  // wake up if energy recovered
  if (p.state == S_SLEEP && p.energy > 70) p.state = S_IDLE;
}

void advanceEvolution(Pet& p, uint32_t dt) {
  if (p.stage >= 4) return;
  p.stageMs += dt;
  if (p.stageMs >= STAGE_DURATION_MS) {
    p.stageMs = 0;
    p.stage++;
    // celebrate
    p.state = S_PLAY;
    p.stateEndMs = millis() + 2500;
    if (&p == &rabbit) p.dialog = rabbitEvolve[random(3)];
    else               p.dialog = bunnyEvolve[random(3)];
    p.lastDialogMs = millis();
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
// Actions (from menu)
// =====================================================================
Pet* currentPet() {
  return (view == V_BUNNY) ? &bunny : &rabbit;
}

void doFeed(Pet& p) {
  clampStat(p.hunger, +30);
  clampStat(p.clean,  -5);
  p.state = S_EAT;
  p.stateEndMs = millis() + 2000;
  p.dialog = (&p == &rabbit) ? rabbitEat[random(3)] : bunnyEat[random(3)];
  p.lastDialogMs = millis();
}

void doPlay(Pet& p) {
  clampStat(p.happy,  +30);
  clampStat(p.energy, -15);
  clampStat(p.hunger, -5);
  p.state = S_PLAY;
  p.stateEndMs = millis() + 2500;
  p.dialog = (&p == &rabbit) ? rabbitPlay[random(3)] : bunnyPlay[random(3)];
  p.lastDialogMs = millis();
}

void doClean(Pet& p) {
  clampStat(p.clean, +40);
  p.state = S_IDLE;
  p.dialog = (&p == &rabbit) ? rabbitClean[random(3)] : bunnyClean[random(3)];
  p.lastDialogMs = millis();
}

void doSleep(Pet& p) {
  p.state = S_SLEEP;
  p.stateEndMs = millis() + 4000;
  clampStat(p.energy, +30);
  p.dialog = (&p == &rabbit) ? rabbitSleep[random(3)] : bunnySleep[random(3)];
  p.lastDialogMs = millis();
}

void doVisit(Pet& p) {
  // current pet goes to friend's house
  visiting = true;
  rabbitVisitingBunny = (&p == &rabbit);
  visitEndMs = millis() + 12000;
  // jump view to the host's house so we see the visit
  view = rabbitVisitingBunny ? V_BUNNY : V_RABBIT;
  p.dialog = (&p == &rabbit) ? rabbitVisit[random(3)] : bunnyVisit[random(3)];
  p.lastDialogMs = millis();
}

void runAction(int idx) {
  Pet& p = *currentPet();
  switch (idx) {
    case 0: doFeed(p);  break;
    case 1: doPlay(p);  break;
    case 2: doClean(p); break;
    case 3: doSleep(p); break;
    case 4: doVisit(p); break;
  }
}

// =====================================================================
// Random outside-meeting events
// =====================================================================
void maybeTriggerOutside() {
  if (outsideEvent || visiting) return;
  if (millis() < nextOutsideCheckMs) return;
  nextOutsideCheckMs = millis() + 60000;  // re-check every minute

  // No outside trips at night
  if (isNight(currentHour)) return;

  // ~25% chance per check during day
  if (random(100) < 25) {
    outsideEvent = true;
    outsideEndMs = millis() + 10000;
    view = V_OUTSIDE;
    rabbit.dialog = rabbitMeet[random(3)];
    bunny.dialog  = bunnyMeet[random(3)];
    rabbit.lastDialogMs = millis();
    bunny.lastDialogMs  = millis();
  }
}

// =====================================================================
// Dialog refresh: switch idle lines every ~6s
// =====================================================================
// Cooldown so a pet doesn't whine every tick about the same low stat
uint32_t rabbitLastComplaintMs = 0;
uint32_t bunnyLastComplaintMs  = 0;

void maybeComplain(Pet& p, uint32_t& lastMs) {
  if (millis() - lastMs < 18000) return;       // 18s cooldown per pet
  if (p.state != S_IDLE && p.state != S_SAD) return;

  // Pick the most urgent unmet need
  int worst = 101;
  int which = -1;   // 0=hunger 1=energy 2=clean 3=happy
  if (p.hunger < 30 && p.hunger < worst) { worst = p.hunger; which = 0; }
  if (p.energy < 25 && p.energy < worst) { worst = p.energy; which = 1; }
  if (p.clean  < 25 && p.clean  < worst) { worst = p.clean;  which = 2; }
  if (p.happy  < 30 && p.happy  < worst) { worst = p.happy;  which = 3; }
  if (which < 0) return;

  bool isRabbit = (&p == &rabbit);
  const char* line = nullptr;
  switch (which) {
    case 0: line = isRabbit ? rabbitHungry[random(3)] : bunnyHungry[random(3)]; break;
    case 1: line = isRabbit ? rabbitTired[random(3)]  : bunnyTired[random(3)];  break;
    case 2: line = isRabbit ? rabbitDirty[random(3)]  : bunnyDirty[random(3)];  break;
    case 3: line = isRabbit ? rabbitBored[random(3)]  : bunnyBored[random(3)];  break;
  }
  p.dialog = line;
  p.lastDialogMs = millis();
  lastMs = millis();
}

void refreshIdleDialog(Pet& p) {
  if (millis() - p.lastDialogMs < 6000) return;
  if (p.state != S_IDLE && p.state != S_SAD) return;
  bool night = isNight(currentHour);
  const char** arr;
  int cnt;
  if (&p == &rabbit) {
    arr = night ? rabbitNight : rabbitIdle;
    cnt = night ? rabbitNightCount : rabbitIdleCount;
  } else {
    arr = night ? bunnyNight : bunnyIdle;
    cnt = night ? bunnyNightCount : bunnyIdleCount;
  }
  p.dialog = arr[random(cnt)];
  p.lastDialogMs = millis();
}

// =====================================================================
// Drawing top-level
// =====================================================================
void drawScene() {
  u8g2.clearBuffer();
  char tbuf[8];
  getTimeString(tbuf, sizeof(tbuf));

  if (view == V_OUTSIDE) {
    drawOutside(currentHour, frame);
    // both pets side by side
    drawRabbit(38, 40, frame, rabbit.stage, S_IDLE);
    drawBunny (80, 40, frame, bunny.stage,  S_IDLE);
    // Status bar shows shared "Outside"
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(2, 8, "Outside");
    u8g2.drawStr(108, 8, tbuf);
    u8g2.drawHLine(0, 10, 128);
    // Alternate dialog between the two friends every ~3s
    bool showRabbit = ((millis() / 3000) & 1) == 0;
    drawDialogBox(showRabbit ? rabbit.dialog : bunny.dialog);
    u8g2.sendBuffer();
    return;
  }

  Pet& p = *currentPet();

  // Background
  if (view == V_RABBIT) drawRabbitHouse(currentHour, frame);
  else                  drawBunnyHouse(currentHour, frame);

  // Top status bar
  char label[24];
  snprintf(label, sizeof(label), "%s (%s)", p.name, stageName(p.stage));
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 8, label);
  u8g2.drawStr(108, 8, tbuf);
  u8g2.drawHLine(0, 10, 128);

  // Stats panel right side (only when menu closed, otherwise menu overlaps)
  if (!menuOpen) {
    drawStatsPanel(p.hunger, p.happy, p.energy, p.clean);
  }

  // Pets in the room
  int pcx = 50, pcy = 40;
  if (view == V_RABBIT) {
    drawRabbit(pcx, pcy, frame, rabbit.stage, rabbit.state);
    // If Bunny is visiting Rabbit's house, draw Bunny too
    if (visiting && !rabbitVisitingBunny) {
      drawBunny(76, pcy, frame, bunny.stage, S_PLAY);
    }
  } else {
    drawBunny(pcx, pcy, frame, bunny.stage, bunny.state);
    if (visiting && rabbitVisitingBunny) {
      drawRabbit(76, pcy, frame, rabbit.stage, S_IDLE);
    }
  }

  // Dialog
  // If a visitor is present, alternate dialog between host and visitor
  const char* line = p.dialog;
  if (visiting) {
    Pet& host    = (rabbitVisitingBunny ? bunny : rabbit);
    Pet& visitor = (rabbitVisitingBunny ? rabbit : bunny);
    line = (((millis() / 3000) & 1) == 0) ? visitor.dialog : host.dialog;
  }
  drawDialogBox(line);

  // Menu overlay
  if (menuOpen) drawActionMenu(menuSel);

  u8g2.sendBuffer();
}

// =====================================================================
// Setup / Loop
// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_C, INPUT_PULLUP);
  pinMode(PIN_BTN_D, INPUT_PULLUP);

  // I2C on the pins our PCB actually uses
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

  // entropy from analog noise on a floating pin
  randomSeed(esp_random());

  connectWifiAndTime();
  updateHour();

  lastFrameMs   = millis();
  lastStatTickMs = millis();
  nextOutsideCheckMs = millis() + 30000;
}

void loop() {
  // ----- input -----
  updateButton(btnA);
  updateButton(btnB);
  updateButton(btnC);
  updateButton(btnD);

  if (btnD.pressedEdge) {
    displayOn = !displayOn;
    u8g2.setPowerSave(displayOn ? 0 : 1);
  }

  if (displayOn) {
    if (menuOpen) {
      if (btnA.pressedEdge) menuSel = (menuSel + 1) % NUM_ACTIONS;
      if (btnB.pressedEdge) { runAction(menuSel); menuOpen = false; }
      if (btnC.pressedEdge) { menuOpen = false; }  // C cancels menu
    } else {
      if (btnA.pressedEdge && view != V_OUTSIDE) { menuOpen = true; menuSel = 0; }
      if (btnB.pressedEdge) {
        // advance/reset idle dialog
        Pet& p = *currentPet();
        p.lastDialogMs = 0;  // force refresh next tick
      }
      if (btnC.pressedEdge) {
        // switch view between the two homes (only if no special scene)
        if (view == V_RABBIT) view = V_BUNNY;
        else if (view == V_BUNNY) view = V_RABBIT;
      }
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

    // Refresh idle dialogs
    refreshIdleDialog(rabbit);
    refreshIdleDialog(bunny);

    // Expire transient state
    if (rabbit.stateEndMs && now > rabbit.stateEndMs) { rabbit.state = S_IDLE; rabbit.stateEndMs = 0; }
    if (bunny.stateEndMs  && now > bunny.stateEndMs)  { bunny.state  = S_IDLE; bunny.stateEndMs  = 0; }

    // Expire visit / outside events
    if (visiting && now > visitEndMs) {
      visiting = false;
    }
    if (outsideEvent && now > outsideEndMs) {
      outsideEvent = false;
      view = V_RABBIT;
    }

    // Randomly trigger outside meetings
    maybeTriggerOutside();
  }

  // Stat decay tick
  if (now - lastStatTickMs >= STAT_TICK_MS) {
    uint32_t delta = now - lastStatTickMs;
    lastStatTickMs = now;
    decayStats(rabbit);
    decayStats(bunny);
    advanceEvolution(rabbit, delta);
    advanceEvolution(bunny,  delta);
    // After stats update, see if either pet needs to voice a need
    maybeComplain(rabbit, rabbitLastComplaintMs);
    maybeComplain(bunny,  bunnyLastComplaintMs);
  }

  // ----- draw -----
  if (displayOn) drawScene();

  delay(15);  // light idle
}
