#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <U8g2lib.h>

// The display object is defined in the main .ino
extern U8G2_SSD1309_128X64_NONAME0_F_HW_I2C u8g2;

// =====================================================================
// Helper drawing primitives
// =====================================================================

// Fill a rectangular area with a 50% checker dither pattern.
// On a monochrome OLED this gives a "gray" look — used for the
// dark/shaded fill of the Black Rabbit.
void drawDither(int x, int y, int w, int h) {
  for (int i = 0; i < w; i++) {
    for (int j = 0; j < h; j++) {
      if (((i + j) & 1) == 0) u8g2.drawPixel(x + i, y + j);
    }
  }
}

// Dither-fill an ellipse centered at (cx,cy) with radii rx,ry.
void drawDitherEllipse(int cx, int cy, int rx, int ry) {
  for (int j = -ry; j <= ry; j++) {
    for (int i = -rx; i <= rx; i++) {
      // ellipse inequality
      if ((long)i * i * ry * ry + (long)j * j * rx * rx <= (long)rx * rx * ry * ry) {
        if (((i + j) & 1) == 0) u8g2.drawPixel(cx + i, cy + j);
      }
    }
  }
}

void drawSun(int x, int y) {
  u8g2.drawDisc(x, y, 3);
  // rays
  u8g2.drawLine(x, y - 5, x, y - 7);
  u8g2.drawLine(x, y + 5, x, y + 7);
  u8g2.drawLine(x - 5, y, x - 7, y);
  u8g2.drawLine(x + 5, y, x + 7, y);
  u8g2.drawPixel(x - 4, y - 4); u8g2.drawPixel(x - 5, y - 5);
  u8g2.drawPixel(x + 4, y - 4); u8g2.drawPixel(x + 5, y - 5);
  u8g2.drawPixel(x - 4, y + 4); u8g2.drawPixel(x - 5, y + 5);
  u8g2.drawPixel(x + 4, y + 4); u8g2.drawPixel(x + 5, y + 5);
}

void drawMoon(int x, int y) {
  u8g2.drawDisc(x, y, 4);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(x + 2, y - 1, 3);
  u8g2.setDrawColor(1);
}

void drawStarsField() {
  // fixed-position stars so they don't shimmer randomly
  static const uint8_t pts[][2] = {
    {10, 6}, {25, 4}, {40, 9}, {55, 5}, {75, 8},
    {95, 4}, {110, 9}, {18, 12}, {68, 13}, {88, 11}
  };
  for (auto &p : pts) u8g2.drawPixel(p[0], p[1]);
}

// =====================================================================
// Pet sizes based on evolution stage
// stage: 0=baby, 1=child, 2=teen, 3=adult, 4=elder
// Returns a 0..3 scaling index used by both sprites.
// =====================================================================
int sizeForStage(int stage) {
  if (stage <= 0) return 0;   // baby - tiny
  if (stage == 1) return 1;   // child
  if (stage == 2) return 2;   // teen
  return 3;                   // adult / elder
}

// =====================================================================
// BLACK RABBIT — dithered fill, tall straight ears, calm eyes.
// state: 0=idle 1=eating 2=playing 3=sleeping 4=sad
// =====================================================================
void drawRabbit(int cx, int cy, int frame, int stage, int state) {
  int s     = sizeForStage(stage);
  int headR = 4 + s;          // head radius
  int bodyW = 5 + s;          // body half-width
  int bodyH = 4 + s;          // body half-height
  int earH  = 5 + s;          // ear height

  // calm breathing: 1px bob
  int yOff = (frame & 1);
  cy += yOff;

  // --- ears (tall straight) — outlined + dithered fill ---
  // left ear
  u8g2.drawVLine(cx - 4, cy - headR - earH, earH);
  u8g2.drawVLine(cx - 2, cy - headR - earH, earH);
  drawDither(cx - 3, cy - headR - earH, 1, earH);
  // right ear
  u8g2.drawVLine(cx + 2, cy - headR - earH, earH);
  u8g2.drawVLine(cx + 4, cy - headR - earH, earH);
  drawDither(cx + 3, cy - headR - earH, 1, earH);

  // --- head: outline + dither interior ---
  int hy = cy - headR / 2;
  u8g2.drawCircle(cx, hy, headR);
  drawDitherEllipse(cx, hy, headR - 1, headR - 1);

  // eyes — quiet (closed/slit even when awake)
  u8g2.setDrawColor(0);
  if (state == 3) {
    // sleeping — same slit
    u8g2.drawHLine(cx - 3, hy - 1, 2);
    u8g2.drawHLine(cx + 2, hy - 1, 2);
  } else {
    u8g2.drawHLine(cx - 3, hy - 1, 2);
    u8g2.drawHLine(cx + 2, hy - 1, 2);
  }
  u8g2.setDrawColor(1);

  // --- body ---
  int by = cy + bodyH / 2 + 1;
  u8g2.drawEllipse(cx, by, bodyW, bodyH);
  drawDitherEllipse(cx, by, bodyW - 1, bodyH - 1);

  // Zzz when sleeping
  if (state == 3) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(cx + headR + 2, hy - 2, "z");
    u8g2.drawStr(cx + headR + 5, hy - 6, "Z");
  }
  // hearts when playing
  if (state == 2 && (frame & 1)) {
    u8g2.drawPixel(cx + headR + 3, hy - 4);
    u8g2.drawPixel(cx + headR + 5, hy - 4);
    u8g2.drawPixel(cx + headR + 2, hy - 3);
    u8g2.drawPixel(cx + headR + 6, hy - 3);
    u8g2.drawPixel(cx + headR + 3, hy - 2);
    u8g2.drawPixel(cx + headR + 5, hy - 2);
    u8g2.drawPixel(cx + headR + 4, hy - 1);
  }
}

// =====================================================================
// WHITE BUNNY — outline only, floppy ears, big bright eyes.
// =====================================================================
void drawBunny(int cx, int cy, int frame, int stage, int state) {
  int s     = sizeForStage(stage);
  int headR = 4 + s;
  int bodyW = 5 + s;
  int bodyH = 4 + s;
  int earH  = 4 + s;

  // bouncy: 2px bob
  int yOff = (frame & 1) ? 2 : 0;
  cy += yOff;

  // --- floppy ears (slightly angled outward) ---
  // left ear
  u8g2.drawLine(cx - 4, cy - headR,           cx - 5, cy - headR - earH);
  u8g2.drawLine(cx - 2, cy - headR,           cx - 3, cy - headR - earH);
  u8g2.drawLine(cx - 5, cy - headR - earH,    cx - 3, cy - headR - earH);
  // right ear
  u8g2.drawLine(cx + 4, cy - headR,           cx + 5, cy - headR - earH);
  u8g2.drawLine(cx + 2, cy - headR,           cx + 3, cy - headR - earH);
  u8g2.drawLine(cx + 3, cy - headR - earH,    cx + 5, cy - headR - earH);

  // --- head outline only (white/light look) ---
  int hy = cy - headR / 2;
  u8g2.drawCircle(cx, hy, headR);

  // --- big eyes ---
  if (state == 3) {
    // sleeping curves
    u8g2.drawHLine(cx - 3, hy - 1, 2);
    u8g2.drawHLine(cx + 2, hy - 1, 2);
  } else {
    u8g2.drawDisc(cx - 2, hy - 1, 1);
    u8g2.drawDisc(cx + 2, hy - 1, 1);
  }

  // smile
  if (state != 3) {
    u8g2.drawPixel(cx - 1, hy + 2);
    u8g2.drawPixel(cx,     hy + 3);
    u8g2.drawPixel(cx + 1, hy + 2);
  }

  // --- body outline ---
  int by = cy + bodyH / 2 + 1;
  u8g2.drawEllipse(cx, by, bodyW, bodyH);
  // little tail bobble
  u8g2.drawCircle(cx + bodyW, by + bodyH / 2, 1);

  if (state == 3) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(cx + headR + 2, hy - 2, "z");
    u8g2.drawStr(cx + headR + 5, hy - 6, "Z");
  }
  if (state == 2 && (frame & 1)) {
    u8g2.drawPixel(cx + headR + 3, hy - 4);
    u8g2.drawPixel(cx + headR + 5, hy - 4);
    u8g2.drawPixel(cx + headR + 2, hy - 3);
    u8g2.drawPixel(cx + headR + 6, hy - 3);
    u8g2.drawPixel(cx + headR + 3, hy - 2);
    u8g2.drawPixel(cx + headR + 5, hy - 2);
    u8g2.drawPixel(cx + headR + 4, hy - 1);
  }
}

// =====================================================================
// Scene backgrounds
// hour 6-19 = day, otherwise night
// =====================================================================
bool isNight(int hour) { return (hour < 6 || hour >= 20); }

void drawCelestial(int x, int y, int hour) {
  if (isNight(hour)) {
    drawMoon(x, y);
    drawStarsField();
  } else {
    drawSun(x, y);
  }
}

// Rabbit's house: tidy. bookshelf left, tea table right, window center.
void drawRabbitHouse(int hour, int frame) {
  // floor
  u8g2.drawHLine(0, 51, 128);
  u8g2.drawHLine(0, 53, 128);

  // bookshelf (left wall)
  u8g2.drawFrame(2, 24, 14, 27);
  u8g2.drawHLine(2, 31, 14);
  u8g2.drawHLine(2, 38, 14);
  u8g2.drawHLine(2, 45, 14);
  // book spines
  for (int i = 4; i < 15; i += 2) {
    u8g2.drawVLine(i, 25, 5);
    u8g2.drawVLine(i, 32, 5);
    u8g2.drawVLine(i, 39, 5);
    u8g2.drawVLine(i, 46, 4);
  }

  // window center-top
  u8g2.drawFrame(48, 16, 32, 18);
  u8g2.drawHLine(48, 24, 32);
  u8g2.drawVLine(63, 16, 18);
  drawCelestial(58, 22, hour);

  // tea table right
  u8g2.drawHLine(102, 46, 22);
  u8g2.drawVLine(106, 46, 5);
  u8g2.drawVLine(120, 46, 5);
  // teacup with steam
  u8g2.drawFrame(110, 40, 7, 6);
  u8g2.drawPixel(117, 41);
  u8g2.drawPixel(118, 42);
  u8g2.drawPixel(117, 43);
  if (frame & 1) {
    u8g2.drawPixel(112, 37);
    u8g2.drawPixel(114, 35);
  } else {
    u8g2.drawPixel(113, 37);
    u8g2.drawPixel(115, 35);
  }
}

// Bunny's house: messy/playful. balloons, scattered toys, window.
void drawBunnyHouse(int hour, int frame) {
  // floor
  u8g2.drawHLine(0, 51, 128);

  // balloons floating top-left (bob with frame)
  int bob = (frame & 1) ? 0 : 1;
  u8g2.drawDisc(8, 20 + bob, 4);
  u8g2.setDrawColor(0); u8g2.drawDisc(8, 20 + bob, 2); u8g2.setDrawColor(1);
  u8g2.drawLine(8, 24 + bob, 8, 35);
  u8g2.drawCircle(18, 16 + bob, 4);
  u8g2.drawLine(18, 20 + bob, 18, 35);
  u8g2.drawDisc(28, 22 + bob, 3);
  u8g2.setDrawColor(0); u8g2.drawDisc(28, 22 + bob, 1); u8g2.setDrawColor(1);
  u8g2.drawLine(28, 25 + bob, 28, 35);

  // window right
  u8g2.drawFrame(78, 16, 28, 18);
  u8g2.drawHLine(78, 24, 28);
  u8g2.drawVLine(92, 16, 18);
  drawCelestial(86, 22, hour);

  // scattered toys on floor
  // ball
  u8g2.drawCircle(38, 48, 3);
  u8g2.drawPixel(37, 47);
  // block
  u8g2.drawFrame(110, 46, 5, 5);
  u8g2.drawPixel(112, 48);
  // tiny carrot
  u8g2.drawLine(120, 48, 122, 50);
  u8g2.drawLine(121, 47, 123, 49);
  u8g2.drawPixel(119, 47);
}

// Outside scene: hill, tree, path, sky element.
void drawOutside(int hour, int frame) {
  // sky element top-right
  drawCelestial(110, 12, hour);

  // tree (left)
  u8g2.drawBox(14, 36, 3, 14);          // trunk
  u8g2.drawDisc(15, 32, 8);             // foliage outline as filled disc
  u8g2.setDrawColor(0);
  // little texture inside
  u8g2.drawDisc(13, 30, 1);
  u8g2.drawDisc(17, 33, 1);
  u8g2.setDrawColor(1);

  // hill / ground (curve)
  for (int x = 0; x < 128; x++) {
    int y = 52 + (int)(2 * sin(x * 0.12));
    u8g2.drawPixel(x, y);
    u8g2.drawPixel(x, y + 1);
  }

  // tiny flowers
  u8g2.drawPixel(45, 50); u8g2.drawPixel(45, 48);
  u8g2.drawPixel(85, 51); u8g2.drawPixel(85, 49);

  // path (dashed line) between left and right of frame
  for (int x = 30; x < 100; x += 4) {
    u8g2.drawHLine(x, 55, 2);
  }
}

// =====================================================================
// UI: top bar, dialog box, action menu, stat bars
// =====================================================================
void drawStatBar(int x, int y, int w, int h, int val) {
  u8g2.drawFrame(x, y, w, h);
  int filled = (val * (w - 2)) / 100;
  if (filled > 0) u8g2.drawBox(x + 1, y + 1, filled, h - 2);
}

// Top status bar: name on left, 4 small bars on right
void drawStatusBar(const char* name, int hunger, int happy, int energy, int clean,
                   const char* timeStr) {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(2, 8, name);
  // time on far right
  u8g2.drawStr(108, 8, timeStr);
  // separator
  u8g2.drawHLine(0, 10, 128);
}

void drawStatsPanel(int hunger, int happy, int energy, int clean) {
  // small bars stacked on right side of screen
  // labels: F P E C
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(98, 17, "F");  drawStatBar(104, 12, 22, 5, hunger);
  u8g2.drawStr(98, 24, "P");  drawStatBar(104, 19, 22, 5, happy);
  u8g2.drawStr(98, 31, "E");  drawStatBar(104, 26, 22, 5, energy);
  u8g2.drawStr(98, 38, "C");  drawStatBar(104, 33, 22, 5, clean);
}

// Dialog box at bottom — speech bubble style
void drawDialogBox(const char* text) {
  u8g2.drawRFrame(0, 54, 128, 10, 1);
  u8g2.setFont(u8g2_font_5x7_tr);
  // center-ish, manually offset
  int w = u8g2.getStrWidth(text);
  int x = (128 - w) / 2;
  if (x < 3) x = 3;
  u8g2.drawStr(x, 62, text);
}

// Action menu overlay (shown when player is choosing)
const char* ACTIONS[] = { "Feed", "Play", "Clean", "Sleep", "Visit" };
const int NUM_ACTIONS = 5;

void drawActionMenu(int selected) {
  // small box top-left under status bar
  u8g2.setDrawColor(0); u8g2.drawBox(2, 13, 58, 38); u8g2.setDrawColor(1);
  u8g2.drawFrame(2, 13, 58, 38);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(8, 20, "ACTIONS");
  u8g2.drawHLine(4, 22, 54);
  for (int i = 0; i < NUM_ACTIONS; i++) {
    int y = 29 + i * 5;
    if (i == selected) {
      u8g2.drawStr(6, y, ">");
      u8g2.drawStr(12, y, ACTIONS[i]);
    } else {
      u8g2.drawStr(12, y, ACTIONS[i]);
    }
  }
}

#endif
