# Rabbit & Bunny — `wildgotchi`

Two-pet tamagotchi for the **wildgotchi** PCB: ESP32-S3-DEVKITC-1-N8R2, 2.42" mono OLED (I2C), four push buttons.

- **Rabbit** (black, dithered fill) — quiet, contemplative, lives among books and tea
- **Bunny** (white, outline) — energetic, bouncy, lives among balloons and toys

They have their own stats, evolve baby → child → teen → adult → elder, visit each other, occasionally meet outside, and speak up when they need something — each in their own voice.

---

## Pin map (from `inzhugotchi.kicad_pcb`)

| Function          | Net      | ESP32-S3 GPIO |
|-------------------|----------|---------------|
| OLED data         | SDA      | **GPIO 8**    |
| OLED clock        | SCL      | **GPIO 9**    |
| Button A (action) | Button1  | **GPIO 15**   |
| Button B (confirm)| Button2  | **GPIO 16**   |
| Button C (switch view) | Button3 | **GPIO 17** |
| Button D (display on/off) | SW4 ON/OFF | **GPIO 18** |

The display connector J4 carries VCC / GND / SDA / SCL. The ON/OFF switch is wired between GPIO 18 and ground — it's software-readable, so the firmware uses it to toggle `u8g2.setPowerSave()` (true sleep of the display, not just blanking).

---

## Controls

| State       | Button A          | Button B           | Button C                | Button D       |
|-------------|-------------------|--------------------|-------------------------|----------------|
| Normal view | Open action menu  | Refresh dialog line| Switch view (R ↔ B)     | Display on/off |
| Action menu | Cycle selection   | Confirm action     | Cancel menu             | Display on/off |

Menu actions: **Feed / Play / Clean / Sleep / Visit**.

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
| `tamagotchi_friends.ino` | Main sketch: state machine, buttons, stat decay, evolution, WiFi/NTP |
| `graphics.h`             | Procedural sprites for both pets, both house interiors, the outdoor scene, UI |
| `dialogs.h`              | Dialog lines per pet, per situation (idle / night / eat / play / clean / sleep / visit / evolve / meet) plus need lines (hungry / tired / dirty / bored) |

All three files live in the `tamagotchi_friends/` folder so the Arduino IDE picks them up as one sketch.

---

## Need-driven dialog

Every 8 s the pets' stats decay. After each tick, each pet checks itself:

- hunger < 30 → speaks a "hungry" line
- energy < 25 → speaks a "tired" line
- clean  < 25 → speaks a "dirty" line
- happy  < 30 → speaks a "bored / lonely" line

Most-urgent need wins. There's an 18 s cooldown so they don't nag every tick. Rabbit's lines are short and quiet (`"Hungry..."`, `"Sleepy..."`); Bunny's are loud (`"FEED ME!!"`, `"BATH PLEASE!!"`).

---

## Tuning

In `tamagotchi_friends.ino`:

```cpp
const uint32_t STAT_TICK_MS      = 8000;             // stat decay interval
const uint32_t STAGE_DURATION_MS = 6UL*60UL*1000UL;  // ~6 min per evolution stage
```

`STAGE_DURATION_MS` = 6 min by default → reaching elder ≈ 24 min of uptime. Crank it up to age them on real-day scale.

---

## Troubleshooting

- **Screen stays blank but board boots fine** → swap `NONAME0` for `NONAME2` in the U8g2 constructor (different RAM mapping on some 2.42" panels).
- **Screen is glitchy / partial** → try lowering `Wire.setClock(400000)` to `100000`.
- **Buttons fire constantly** → check that each SW pin is wired GPIO ↔ GND and that internal pull-ups are taking effect (they are, via `INPUT_PULLUP`).
- **Time stays at midnight** → WiFi didn't connect; check SSID/password. Game still runs, just with a drifting fake clock.

---

## Known limitations

- No persistent save — power-cycle resets the pets. (NVS save is a small future addition.)
- Sprites are procedural; replace `drawRabbit` / `drawBunny` with XBM bitmaps if you want fancier pixel art.
- Single WiFi sync at boot. Hour-resolution accuracy holds easily across many hours.
