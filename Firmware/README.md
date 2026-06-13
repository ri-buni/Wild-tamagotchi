# ALL FIRMWARE WAS MADE BY CLAUDE CODE

# Rabbit & Bunny — `wildgotchi`

Two-pet tamagotchi for the **wildgotchi** PCB: ESP32-S3-DEVKITC-1-N8R2, 2.42" mono OLED (I2C), four push buttons.

- **Rabbit** (black, dithered fill) — quiet, contemplative, lives among books and tea
- **Bunny** (white, outline) — energetic, bouncy, lives among balloons and toys

They have their own stats, evolve baby → child → teen → adult → elder, walk a shared world of five locations, visit each other's houses, occasionally meet out in the meadow, and speak up when they need something — each in their own voice.

---

## Pin map (from `inzhugotchi.kicad_pcb`)

| Function          | Net      | ESP32-S3 GPIO |
|-------------------|----------|---------------|
| OLED data         | SDA      | **GPIO 8**    |
| OLED clock        | SCL      | **GPIO 9**    |
| Button 1 (left)   | Button1  | **GPIO 15**   |
| Button 2 (right)  | Button2  | **GPIO 16**   |
| Button 3 (action) | Button3  | **GPIO 17**   |
| Button 4 (switch / power) | SW4 | **GPIO 18** |

The display connector J4 carries VCC / GND / SDA / SCL. Every button is wired GPIO ↔ GND and read with the ESP32's internal pull-ups (`INPUT_PULLUP`), so a press reads LOW. SW4 is software-readable, so the firmware uses a **double-press** on it to toggle `u8g2.setPowerSave()` (true sleep of the display, not just blanking).

---

## Controls

The world is a row of five locations. **Left / Right walk the active pet** between them; the screen always follows whoever you're controlling.

```
[ R.House ] — [ Garden ] — [ Meadow ] — [ Pond ] — [ B.House ]
```

| Context     | Button 1 (LEFT) | Button 2 (RIGHT) | Button 3 (ACTION) | Button 4 (SWITCH)          |
|-------------|-----------------|------------------|-------------------|----------------------------|
| Normal view | Walk left       | Walk right       | Open action menu  | 1 press: switch Rabbit ↔ Bunny |
| Action menu | Selection up    | Selection down   | Confirm selection | 1 press: cancel menu       |
| Any time    | —               | —                | —                 | **2 fast presses: display on/off** |

**Button 4** does triple duty. A single press switches which pet you control (or backs out of the menu); two quick presses toggle the screen. The single-press is resolved after a ~350 ms window so it can tell a single tap from the start of a double tap — that's the only intentional input lag in the firmware.

Menu actions: **Feed / Play / Clean / Sleep / Back**. (`Back` just closes the menu; pressing Button 4 does the same.)

---

## Locations

Walking the active pet left/right moves them one step along the map. Each spot has its own drawn scene and gives one action a bonus:

| Location  | Scene                                   | Bonus                       |
|-----------|-----------------------------------------|-----------------------------|
| R.House   | Bookshelf, tea table, window            | Sleep (home) → +40 energy   |
| Garden    | Fence, carrot rows, watering can        | **Feed → +45 hunger**       |
| Meadow    | Tree, rolling hills, flowers, path      | **Play → +45 happy**        |
| Pond      | Rippling water, reeds, jumping fish      | **Clean → +60 clean**       |
| B.House   | Balloons, scattered toys, window        | Sleep (home) → +40 energy   |

Walking into the *other* pet's house counts as **visiting**. When both pets share a location they greet each other, get a small happiness boost, and their dialog alternates every ~3 s. A small ▼ marker above one pet shows which one your buttons are controlling.

Away from home, **Feed/Play/Clean** still work at normal strength; **Sleep** away from home gives +25 energy instead of +40.

---

## Setup

1. Arduino IDE 2.x with ESP32 board support installed (esp32 by Espressif).
2. Library Manager → install **U8g2 by oliver**.
3. Edit `WIFI_SSID` / `WIFI_PASS` at the top of `tamagotchi_friends.ino`. WiFi is used once at boot to sync time over NTP (so the in-game day/night cycle matches Almaty real time). Radio is then turned off.
4. Board: **ESP32S3 Dev Module**. USB CDC On Boot: Enabled.
5. Upload.

If WiFi fails the clock just starts at noon and drifts — everything else still works.

---

## Files

| File                     | What it contains |
|--------------------------|------------------|
| `tamagotchi_friends.ino` | Main sketch: state machine, buttons (incl. single/double-press resolver), movement between locations, stat decay, evolution, WiFi/NTP |
| `graphics.h`             | `Location` enum, `Display` typedef, procedural sprites for both pets, all five location scenes, nav arrows, active-pet marker, UI |
| `dialogs.h`              | Dialog lines per pet, per situation — symmetric across both pets |

All three files live in the `tamagotchi_friends/` folder so the Arduino IDE picks them up as one sketch.

---

## Dialog

Each pet has its own voice. Rabbit's lines are short and quiet (`"Hungry..."`, `"Sleepy..."`); Bunny's are loud (`"FEED ME!!"`, `"BATH PLEASE!!"`). Every category exists for **both** pets:

- **idle / night** — ambient chatter, day vs. night variants
- **eat / play / clean / sleep** — reactions to each action
- **visit / meet / evolve** — social and growth moments
- **hungry / tired / dirty / bored** — need-driven complaints (see below)
- **home / garden / meadow / pond** — spoken on arrival at each location
- **feedGarden / playMeadow / cleanPond / sleepAway** — said when an action lands on its bonus spot (or sleep happens away from home)

### Need-driven dialog

Every 8 s the pets' stats decay. After each tick, each pet checks itself:

- hunger < 30 → speaks a "hungry" line
- energy < 25 → speaks a "tired" line
- clean  < 25 → speaks a "dirty" line
- happy  < 30 → speaks a "bored / lonely" line

Most-urgent need wins. There's an 18 s cooldown so they don't nag every tick.

---

## Tuning

In `tamagotchi_friends.ino`:

```cpp
const uint32_t STAT_TICK_MS      = 8000;             // stat decay interval
const uint32_t STAGE_DURATION_MS = 6UL*60UL*1000UL;  // ~6 min per evolution stage
const uint32_t DOUBLE_PRESS_MS   = 350;              // Button 4 single/double window
```

`STAGE_DURATION_MS` = 6 min by default → reaching elder ≈ 24 min of uptime. Crank it up to age them on real-day scale. Raise `DOUBLE_PRESS_MS` if double-presses don't register, lower it to shorten the single-press delay.

---

## Troubleshooting

- **Screen stays blank but board boots fine** → change the `Display` typedef at the top of `graphics.h` from `NONAME0` to `NONAME2` (different RAM mapping on some 2.42" panels). It's now a single line, used everywhere.
- **Screen is glitchy / partial** → try lowering `Wire.setClock(400000)` to `100000`.
- **Buttons fire constantly** → check that each SW pin is wired GPIO ↔ GND and that internal pull-ups are taking effect (they are, via `INPUT_PULLUP`).
- **Single press of Button 4 feels laggy** → that's the double-press window; lower `DOUBLE_PRESS_MS`.
- **Time stays at midnight** → WiFi didn't connect; check SSID/password. Game still runs, just with a drifting fake clock.

---

## Known limitations

- No persistent save — power-cycle resets the pets, including their location. (NVS save is a small future addition.)
- Sprites are procedural; replace `drawRabbit` / `drawBunny` with XBM bitmaps if you want fancier pixel art.
- Single WiFi sync at boot. Hour-resolution accuracy holds easily across many hours.
