#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <U8g2lib.h>

// The display object is defined in the main .ino.
// If your panel needs the NONAME2 RAM mapping, change ONLY this typedef.
typedef U8G2_SSD1309_128X64_NONAME0_F_HW_I2C Display;
extern Display u8g2;

// =====================================================================
// World map — a row of 5 locations. LEFT/RIGHT walks the active pet
// through them.  Rabbit's house is the left end, Bunny's the right end.
// =====================================================================
enum Location {
  LOC_RHOME = 0,   // Rabbit's house (left end)
  LOC_GARDEN,      // carrot garden        (feed bonus)
  LOC_MEADOW,      // open meadow          (play bonus, meet spot)
  LOC_POND,        // pond                 (clean bonus)
  LOC_BHOME,       // Bunny's house (right end)
  NUM_LOCS
};

const char* LOC_NAMES[NUM_LOCS] = { "R.House", "Garden", "Meadow", "Pond", "B.House" };

// =====================================================================
// Helper drawing primitives
// =====================================================================

// Fill a rectangular area with a 50% checker dither pattern.
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
      if ((long)i * i * ry * ry + (long)j * j * rx * rx <= (long)rx * rx * ry * ry) {
        if (((i + j) & 1) == 0) u8g2.drawPixel(cx + i, cy + j);
      }
    }
  }
}

void drawSun(int x, int y) {
  u8g2.drawDisc(x, y, 3);
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
  static const uint8_t pts[][2] = {
    {10, 6}, {25, 4}, {40, 9}, {55, 5}, {75, 8},
    {95, 4}, {110, 9}, {18, 12}, {68, 13}, {88, 11}
  };
  for (auto &p : pts) u8g2.drawPixel(p[0], p[1]);
}

// =====================================================================
// Pet sizes based on evolution stage
// =====================================================================
int sizeForStage(int stage) {
  if (stage <= 0) return 0;   // baby - tiny
  if (stage == 1) return 1;   // child
  if (stage == 2) return 2;   // teen
  return 3;                   // adult / elder
}

// =====================================================================
// BLACK RABBIT — dithered fill, tall straight ears, calm slit eyes.
// state: 0=idle 1=eating 2=playing 3=sleeping 4=sad
// =====================================================================
void drawRabbit(int cx, int cy, int frame, int stage, int state) {
  int s     = sizeForStage(stage);
  int headR = 4 + s;
  int bodyW = 5 + s;
  int bodyH = 4 + s;
  int earH  = 5 + s;

  int yOff = (frame & 1);   // calm 1px breathing bob
  cy += yOff;

  // ears (tall, straight)
  u8g2.drawVLine(cx - 4, cy - headR - earH, earH);
  u8g2.drawVLine(cx - 2, cy - headR - earH, earH);
  drawDither(cx - 3, cy - headR - earH, 1, earH);
  u8g2.drawVLine(cx + 2, cy - headR - earH, earH);
  u8g2.drawVLine(cx + 4, cy - headR - earH, earH);
  drawDither(cx + 3, cy - headR - earH, 1, earH);

  // head
  int hy = cy - headR / 2;
  u8g2.drawCircle(cx, hy, headR);
  drawDitherEllipse(cx, hy, headR - 1, headR - 1);

  // eyes — the Rabbit keeps the same quiet slit eyes awake or asleep,
  // which is the joke (the old code had two identical branches here)
  u8g2.setDrawColor(0);
  u8g2.drawHLine(cx - 3, hy - 1, 2);
  u8g2.drawHLine(cx + 2, hy - 1, 2);
  u8g2.setDrawColor(1);

  // body
  int by = cy + bodyH / 2 + 1;
  u8g2.drawEllipse(cx, by, bodyW, bodyH);
  drawDitherEllipse(cx, by, bodyW - 1, bodyH - 1);

  if (state == 3) {
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(cx + headR + 2, hy - 2, "z");
    u8g2.drawStr(cx + headR + 5, hy - 6, "Z");
  }
  if (state == 2 && (frame & 1)) {   // tiny heart when playing
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

  int yOff = (frame & 1) ? 2 : 0;   // bouncy 2px bob
  cy += yOff;

  // floppy ears
  u8g2.drawLine(cx - 4, cy - headR,        cx - 5, cy - headR - earH);
  u8g2.drawLine(cx - 2, cy - headR,        cx - 3, cy - headR - earH);
  u8g2.drawLine(cx - 5, cy - headR - earH, cx - 3, cy - headR - earH);
  u8g2.drawLine(cx + 4, cy - headR,        cx + 5, cy - headR - earH);
  u8g2.drawLine(cx + 2, cy - headR,        cx + 3, cy - headR - earH);
  u8g2.drawLine(cx + 3, cy - headR - earH, cx + 5, cy - headR - earH);

  // head outline
  int hy = cy - headR / 2;
  u8g2.drawCircle(cx, hy, headR);

  // eyes
  if (state == 3) {
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

  // body + tail
  int by = cy + bodyH / 2 + 1;
  u8g2.drawEllipse(cx, by, bodyW, bodyH);
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
  u8g2.drawHLine(0, 51, 128);
  u8g2.drawHLine(0, 53, 128);

  // bookshelf
  u8g2.drawFrame(2, 24, 14, 27);
  u8g2.drawHLine(2, 31, 14);
  u8g2.drawHLine(2, 38, 14);
  u8g2.drawHLine(2, 45, 14);
  for (int i = 4; i < 15; i += 2) {
    u8g2.drawVLine(i, 25, 5);
    u8g2.drawVLine(i, 32, 5);
    u8g2.drawVLine(i, 39, 5);
    u8g2.drawVLine(i, 46, 4);
  }

  // window
  u8g2.drawFrame(48, 16, 32, 18);
  u8g2.drawHLine(48, 24, 32);
  u8g2.drawVLine(63, 16, 18);
  drawCelestial(58, 22, hour);

  // tea table + teacup with steam
  u8g2.drawHLine(102, 46, 22);
  u8g2.drawVLine(106, 46, 5);
  u8g2.drawVLine(120, 46, 5);
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
  u8g2.drawHLine(0, 51, 128);

  int bob = (frame & 1) ? 0 : 1;
  u8g2.drawDisc(8, 20 + bob, 4);
  u8g2.setDrawColor(0); u8g2.drawDisc(8, 20 + bob, 2); u8g2.setDrawColor(1);
  u8g2.drawLine(8, 24 + bob, 8, 35);
  u8g2.drawCircle(18, 16 + bob, 4);
  u8g2.drawLine(18, 20 + bob, 18, 35);
  u8g2.drawDisc(28, 22 + bob, 3);
  u8g2.setDrawColor(0); u8g2.drawDisc(28, 22 + bob, 1); u8g2.setDrawColor(1);
  u8g2.drawLine(28, 25 + bob, 28, 35);

  u8g2.drawFrame(78, 16, 28, 18);
  u8g2.drawHLine(78, 24, 28);
  u8g2.drawVLine(92, 16, 18);
  drawCelestial(86, 22, hour);

  // scattered toys
  u8g2.drawCircle(38, 48, 3);
  u8g2.drawPixel(37, 47);
  u8g2.drawFrame(110, 46, 5, 5);
  u8g2.drawPixel(112, 48);
  u8g2.drawLine(120, 48, 122, 50);
  u8g2.drawLine(121, 47, 123, 49);
  u8g2.drawPixel(119, 47);
}

// Meadow (the old "outside"): hill, tree, path, sky element.
void drawMeadow(int hour, int frame) {
  (void)frame;  // static scene; frame kept for a uniform signature
  drawCelestial(110, 12, hour);

  // tree
  u8g2.drawBox(14, 36, 3, 14);
  u8g2.drawDisc(15, 32, 8);
  u8g2.setDrawColor(0);
  u8g2.drawDisc(13, 30, 1);
  u8g2.drawDisc(17, 33, 1);
  u8g2.setDrawColor(1);

  // rolling ground
  for (int x = 0; x < 128; x++) {
    int y = 52 + (int)(2 * sin(x * 0.12));
    u8g2.drawPixel(x, y);
    u8g2.drawPixel(x, y + 1);
  }

  // flowers
  u8g2.drawPixel(45, 50); u8g2.drawPixel(45, 48);
  u8g2.drawPixel(85, 51); u8g2.drawPixel(85, 49);

  // path
  for (int x = 30; x < 100; x += 4) {
    u8g2.drawHLine(x, 55, 2);
  }
}

// Garden: fence along the back, rows of carrots, a watering can.
void drawGarden(int hour, int frame) {
  drawCelestial(14, 16, hour);
  u8g2.drawHLine(0, 51, 128);

  // fence
  for (int x = 40; x <= 124; x += 12) {
    u8g2.drawVLine(x, 26, 12);          // post
    u8g2.drawPixel(x - 1, 26);
    u8g2.drawPixel(x + 1, 26);
  }
  u8g2.drawHLine(38, 29, 90);           // rails
  u8g2.drawHLine(38, 34, 90);

  // carrot rows: leaves above ground, root hinted below the soil line
  for (int x = 8; x <= 120; x += 14) {
    // leaves (small V), waving with the animation frame
    int w = ((x / 14 + frame) & 1);
    u8g2.drawLine(x, 46, x - 2 + w, 43);
    u8g2.drawLine(x, 46, x + 2 - w, 43);
    u8g2.drawVLine(x, 46, 3);           // carrot top peeking out
    u8g2.drawPixel(x, 50);
  }

  // soil furrows
  for (int x = 2; x < 126; x += 6) u8g2.drawPixel(x, 49);

  // watering can, bottom-left
  u8g2.drawFrame(2, 42, 8, 7);
  u8g2.drawLine(10, 44, 13, 41);        // spout
  u8g2.drawPixel(14, 40);
  u8g2.drawCircle(6, 41, 2);            // handle (top half visible)
}

// Pond: water with animated ripples, reeds, a lily pad.
void drawPond(int hour, int frame) {
  drawCelestial(112, 14, hour);
  u8g2.drawHLine(0, 51, 128);

  // water
  u8g2.drawEllipse(50, 47, 36, 5);

  // ripples shimmer with the frame
  if (frame & 1) {
    u8g2.drawHLine(34, 46, 6);
    u8g2.drawHLine(56, 48, 7);
    u8g2.drawHLine(70, 46, 5);
  } else {
    u8g2.drawHLine(28, 47, 5);
    u8g2.drawHLine(48, 45, 6);
    u8g2.drawHLine(62, 48, 6);
  }

  // lily pad
  u8g2.drawEllipse(40, 49, 4, 1);

  // reeds, right bank
  u8g2.drawVLine(92, 36, 15);
  u8g2.drawVLine(96, 33, 18);
  u8g2.drawVLine(100, 38, 13);
  u8g2.drawBox(95, 33, 3, 4);           // cattail head
  u8g2.drawBox(91, 36, 3, 3);

  // little fish jumping occasionally
  if ((frame & 7) == 0) {
    u8g2.drawPixel(58, 42);
    u8g2.drawPixel(59, 41);
    u8g2.drawPixel(60, 42);
  }
}

// Dispatch: draw the background for a location.
void drawLocation(int loc, int hour, int frame) {
  switch (loc) {
    case LOC_RHOME:  drawRabbitHouse(hour, frame); break;
    case LOC_GARDEN: drawGarden(hour, frame);      break;
    case LOC_MEADOW: drawMeadow(hour, frame);      break;
    case LOC_POND:   drawPond(hour, frame);        break;
    case LOC_BHOME:  drawBunnyHouse(hour, frame);  break;
  }
}

// =====================================================================
// UI: dialog box, action menu, stat bars, navigation hints
// =====================================================================
void drawStatBar(int x, int y, int w, int h, int val) {
  u8g2.drawFrame(x, y, w, h);
  int filled = (val * (w - 2)) / 100;
  if (filled > 0) u8g2.drawBox(x + 1, y + 1, filled, h - 2);
}

void drawStatsPanel(int hunger, int happy, int energy, int clean) {
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(98, 17, "F");  drawStatBar(104, 12, 22, 5, hunger);
  u8g2.drawStr(98, 24, "P");  drawStatBar(104, 19, 22, 5, happy);
  u8g2.drawStr(98, 31, "E");  drawStatBar(104, 26, 22, 5, energy);
  u8g2.drawStr(98, 38, "C");  drawStatBar(104, 33, 22, 5, clean);
}

// "< Garden >" navigation hints at the screen edges, mid-height.
// Only the arrows that are actually walkable are shown.
void drawNavHints(int loc) {
  u8g2.setFont(u8g2_font_5x7_tr);
  if (loc > 0)            u8g2.drawStr(0, 36, "<");
  if (loc < NUM_LOCS - 1) u8g2.drawStr(124, 36, ">");
}

// Small down-arrow marker above the active pet (shown when both pets
// share a location so you know which one your buttons control).
void drawActiveMarker(int cx, int y) {
  u8g2.drawHLine(cx - 2, y,     5);
  u8g2.drawHLine(cx - 1, y + 1, 3);
  u8g2.drawPixel(cx,     y + 2);
}

// Dialog box at bottom — speech bubble style
void drawDialogBox(const char* text) {
  u8g2.drawRFrame(0, 54, 128, 10, 1);
  u8g2.setFont(u8g2_font_5x7_tr);
  int w = u8g2.getStrWidth(text);
  int x = (128 - w) / 2;
  if (x < 3) x = 3;
  u8g2.drawStr(x, 62, text);
}

// Action menu overlay (LEFT/RIGHT to move, ACTION to confirm).
// "Back" closes the menu; pressing the pet-switch button also cancels.
const char* ACTIONS[] = { "Feed", "Play", "Clean", "Sleep", "Back" };
const int NUM_ACTIONS = 5;
const int ACTION_BACK = 4;

void drawActionMenu(int selected) {
  u8g2.setDrawColor(0); u8g2.drawBox(2, 13, 58, 38); u8g2.setDrawColor(1);
  u8g2.drawFrame(2, 13, 58, 38);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(8, 20, "ACTIONS");
  u8g2.drawHLine(4, 22, 54);
  for (int i = 0; i < NUM_ACTIONS; i++) {
    int y = 29 + i * 5;
    if (i == selected) u8g2.drawStr(6, y, ">");
    u8g2.drawStr(12, y, ACTIONS[i]);
  }
}

#endif
