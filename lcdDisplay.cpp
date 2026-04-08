#include "lcdDisplay.h"

// ============================================================
//  CONSTRUCTOR / BEGIN
// ============================================================
LCDDISPLAY::LCDDISPLAY() {
  tft = new Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
}

void LCDDISPLAY::begin() {
  tft->init(240, 320);
  tft->setRotation(0);
  tft->fillScreen(C_BLACK);
}

// ============================================================
//  STATIC PRICING UTILITIES
// ============================================================

// Minutes → pesos  (ceil so customer always pays enough)
// ceil(minutes / MINS_PER_PESO)  using integer arithmetic:
//   = (minutes + MINS_PER_PESO - 1) / MINS_PER_PESO
// Examples:
//   15 min → (15+2)/3 = 5   → P5
//   25 min → (25+2)/3 = 9   → P9
//   30 min → (30+2)/3 = 10  → P10
//   60 min → (60+2)/3 = 20  → P20
int LCDDISPLAY::minutesToPesos(int minutes) {
  if (minutes <= 0) return 0;
  return (minutes + MINS_PER_PESO - 1) / MINS_PER_PESO;
}

// Pesos → minutes  (exact, no rounding — every peso = 3 min)
// Examples:
//   P1  →   3 min
//   P5  →  15 min
//   P7  →  21 min
//   P10 →  30 min
//   P20 →  60 min
//   P40 → 120 min
//   P65 → 195 min  =  3h 15m
int LCDDISPLAY::pesosToMinutes(int pesos) {
  if (pesos <= 0) return 0;
  return pesos * MINS_PER_PESO;
}

// Keypad entry is valid if within allowed range
bool LCDDISPLAY::isValidMinutes(int minutes) {
  return (minutes >= MIN_MINUTES && minutes <= MAX_MINUTES);
}

// ============================================================
//  INTERNAL HELPERS
// ============================================================
void LCDDISPLAY::drawHeader(uint16_t color, const char* line1, const char* line2) {
  int h = (line2 == nullptr) ? 42 : 54;
  tft->fillRect(0, 0, 240, h, color);
  // Contrast: dark background → white text; light (green/orange) → black
  uint16_t tc = (color == C_GREEN || color == C_ORANGE || color == C_YELLOW)
                ? C_BLACK : C_WHITE;
  tft->setTextColor(tc);
  tft->setTextSize(2);
  tft->setCursor(10, (line2 == nullptr) ? 13 : 7);
  tft->print(line1);
  if (line2 != nullptr) {
    tft->setCursor(10, 33);
    tft->print(line2);
  }
}

void LCDDISPLAY::drawProgressBar(int x, int y, int w, int h,
                                  int value, int maxVal, uint16_t fillColor) {
  tft->drawRect(x, y, w, h, C_DKGRAY);
  if (maxVal <= 0) return;
  int fill = map(constrain(value, 0, maxVal), 0, maxVal, 0, w - 4);
  if (fill > 0) tft->fillRect(x + 2, y + 2, fill, h - 4, fillColor);
}

// Print "Xh Ym" if >= 60 min, otherwise "X min"
void LCDDISPLAY::printDuration(int totalMinutes) {
  if (totalMinutes >= 60) {
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    tft->print(h);
    tft->print("h");
    if (m > 0) {
      tft->print(" ");
      tft->print(m);
      tft->print("m");
    }
  } else {
    tft->print(totalMinutes);
    tft->print(" min");
  }
}

// ============================================================
//  WELCOME SCREEN
// ============================================================
void LCDDISPLAY::showWelcome() {
  tft->fillScreen(C_BLACK);

  drawHeader(C_BLUE, "eBike Charging", "Station  v1.0");

  // Rate banner
  tft->fillRect(0, 56, 240, 24, C_DKGRAY);
  tft->setTextColor(C_YELLOW);
  tft->setTextSize(2);
  tft->setCursor(12, 61);
  tft->print("P1 = ");
  tft->print(MINS_PER_PESO);
  tft->print(" mins  |  min: P1");

  // Quick select label
  tft->setTextColor(C_CYAN);
  tft->setTextSize(1);
  tft->setCursor(10, 88);
  tft->print("---- Quick Select ----");

  // Shortcut rows
  struct { char key; int mins; } sc[] = {
    {'A', SHORTCUT_A_MINS},
    {'B', SHORTCUT_B_MINS},
    {'C', SHORTCUT_C_MINS}
  };
  int y = 100;
  for (int i = 0; i < 3; i++) {
    tft->setCursor(10, y);
    tft->setTextColor(C_YELLOW);
    tft->setTextSize(2);
    tft->print("[");
    tft->print(sc[i].key);
    tft->print("]");
    tft->setTextColor(C_WHITE);
    tft->print(" ");
    tft->print(sc[i].mins);
    tft->print("min = P");
    tft->print(minutesToPesos(sc[i].mins));
    y += 28;
  }

  // Divider
  tft->drawLine(0, y + 4, 240, y + 4, C_DKGRAY);
  y += 12;

  // Custom entry label
  tft->setTextColor(C_CYAN);
  tft->setTextSize(1);
  tft->setCursor(10, y);
  tft->print("---- Custom Entry ----");
  y += 13;

  tft->setTextColor(C_LTGRAY);
  tft->setCursor(10, y);
  tft->print("Type any minutes + [#]");
  y += 12;
  tft->setCursor(10, y);
  tft->print("Any peso amount accepted");
  y += 12;

  // Example conversions
  tft->setTextColor(C_DKGRAY);
  tft->setCursor(10, y);
  tft->print("P7=21m  P25=75m  P65=3h15m");

  // Static prompt beside animation icon zone (y=252..298)
  tft->setTextColor(C_DKGRAY);
  tft->setTextSize(1);
  tft->setCursor(80, 288);
  tft->print("Press any key...");

  // Footer
  tft->fillRect(0, 302, 240, 18, C_DKGRAY);
  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(28, 307);
  tft->print("Solar Powered Station");

  // Prime first animation frame immediately
  _homeAnimFrame = 0;
  _homeAnimLast  = 0;
}

// ============================================================
//  SELECTION SCREEN
// ============================================================
void LCDDISPLAY::showSelectScreen(String inputBuffer, int previewMinutes) {
  tft->fillScreen(C_BLACK);

  drawHeader(C_BLUE, "Enter Duration");

  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 50);
  tft->print("[#] confirm  [*] delete  [A/B/C] quick");

  // Input box
  tft->drawRect(10, 62, 220, 52, C_YELLOW);
  tft->setTextSize(4);
  if (inputBuffer.length() > 0) {
    tft->setTextColor(C_YELLOW);
    tft->setCursor(16, 73);
    tft->print(inputBuffer);
    tft->print(" min");
  } else {
    tft->setTextColor(C_DKGRAY);
    tft->setCursor(16, 73);
    tft->print("___ min");
  }

  // Price preview box
  tft->drawRect(10, 124, 220, 70, C_CYAN);

  if (previewMinutes >= MIN_MINUTES && previewMinutes <= MAX_MINUTES) {
    int pesos = minutesToPesos(previewMinutes);

    tft->setTextColor(C_CYAN);
    tft->setTextSize(1);
    tft->setCursor(18, 131);
    tft->print("Insert:");

    // Big peso amount
    tft->setTextColor(C_WHITE);
    tft->setTextSize(4);
    tft->setCursor(18, 143);
    tft->print("P");
    tft->print(pesos);

    // Duration confirmation on the right
    tft->setTextColor(C_LTGRAY);
    tft->setTextSize(1);
    int labelX = (pesos >= 100) ? 170 : (pesos >= 10) ? 140 : 116;
    tft->setCursor(labelX, 148);
    tft->print("for ");
    tft->setCursor(labelX, 160);
    printDuration(previewMinutes);

  } else if (previewMinutes > MAX_MINUTES) {
    tft->setTextColor(C_RED);
    tft->setTextSize(1);
    tft->setCursor(18, 148);
    tft->print("Max is ");
    tft->print(MAX_MINUTES);
    tft->print(" min");
    tft->setCursor(18, 162);
    tft->print("(P");
    tft->print(minutesToPesos(MAX_MINUTES));
    tft->print(")");

  } else if (inputBuffer.length() > 0 && previewMinutes < MIN_MINUTES) {
    tft->setTextColor(C_RED);
    tft->setTextSize(1);
    tft->setCursor(18, 148);
    tft->print("Min is ");
    tft->print(MIN_MINUTES);
    tft->print(" min (P1)");

  } else {
    tft->setTextColor(C_DKGRAY);
    tft->setTextSize(1);
    tft->setCursor(18, 154);
    tft->print("Price will show here");
  }

  // Rate reminder + shortcut ref
  tft->setTextColor(C_DKGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 204);
  tft->print("Rate: P1 = ");
  tft->print(MINS_PER_PESO);
  tft->print(" min  |  A=15  B=30  C=60");
}

// ============================================================
//  COIN WAITING SCREEN  —  STATIC LAYOUT  (call ONCE on entry)
//
//  Draws the full screen skeleton: header, "Required" label,
//  static hint text, and the "Time granted:" label.
//  Then calls updateCoinWaiting() for the live parts.
//  The coin animation zone (x=165,y=218,w=65,h=34) is left
//  blank here — tickCoinAnim() owns it exclusively.
// ============================================================
void LCDDISPLAY::showCoinWaiting(int requiredPesos, int coinsInserted, int minutesGranted) {
  tft->fillScreen(C_BLACK);

  drawHeader(C_ORANGE, "Insert Coins");

  // "Required: Pxx"  — never changes during this screen
  tft->setTextColor(C_YELLOW);
  tft->setTextSize(2);
  tft->setCursor(10, 52);
  tft->print("Required: P");
  tft->print(requiredPesos);

  // Static label above the live time value
  tft->setTextColor(C_CYAN);
  tft->setTextSize(1);
  tft->setCursor(10, 180);
  tft->print("Time granted:");

  // Bottom hint — never changes
  tft->setTextColor(C_DKGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 255);
  tft->print("Every P1 = ");
  tft->print(MINS_PER_PESO);
  tft->print(" min  |  [STOP] cancel");

  // Draw live values for the first time
  updateCoinWaiting(requiredPesos, coinsInserted, minutesGranted);

  // Prime animation — first frame draws immediately on next tick
  _coinAnimFrame = 0;
  _coinAnimLast  = 0;
}

// ============================================================
//  COIN WAITING SCREEN  —  DYNAMIC UPDATE  (call on each coin)
//
//  Only redraws the three regions that change per coin insert:
//    1. Progress bar          y=78..103
//    2. Inserted / Need text  y=115..165
//    3. Time granted value    y=193..232
//
//  Everything else (header, labels, hint, animation zone)
//  is untouched — zero flicker on the rest of the screen.
// ============================================================
void LCDDISPLAY::updateCoinWaiting(int requiredPesos, int coinsInserted, int minutesGranted) {

  // ----------------------------------------------------------
  //  1. Progress bar
  // ----------------------------------------------------------
  tft->fillRect(10, 78, 220, 26, C_BLACK);          // erase old bar
  drawProgressBar(10, 78, 220, 26, coinsInserted, requiredPesos, C_GREEN);

  // ----------------------------------------------------------
  //  2. Inserted amount
  // ----------------------------------------------------------
  tft->fillRect(10, 115, 220, 24, C_BLACK);          // erase old value
  tft->setTextColor(C_WHITE);
  tft->setTextSize(2);
  tft->setCursor(10, 115);
  tft->print("Inserted: P");
  tft->print(coinsInserted);

  // ----------------------------------------------------------
  //  3. Still needed  (colour flips green when fully paid)
  // ----------------------------------------------------------
  int stillNeed = requiredPesos - coinsInserted;
  if (stillNeed < 0) stillNeed = 0;

  tft->fillRect(10, 142, 220, 24, C_BLACK);          // erase old value
  tft->setTextColor(stillNeed > 0 ? C_YELLOW : C_GREEN);
  tft->setTextSize(2);
  tft->setCursor(10, 142);
  tft->print("Need:     P");
  tft->print(stillNeed);

  // Divider — cheap to redraw, keeps it crisp
  tft->drawLine(0, 172, 240, 172, C_DKGRAY);

  // ----------------------------------------------------------
  //  4. Time granted value
  //     "Time granted:" label is static (drawn in showCoinWaiting)
  //     Only the number below it changes here.
  // ----------------------------------------------------------
  tft->fillRect(10, 193, 150, 34, C_BLACK);          // erase old value
                                                      // width=150 avoids coin anim zone (x=165)
  tft->setTextSize(3);
  tft->setCursor(10, 193);
  if (minutesGranted > 0) {
    tft->setTextColor(C_WHITE);
    printDuration(minutesGranted);
  } else {
    tft->setTextColor(C_DKGRAY);
    tft->print("--");
  }
}

// ============================================================
//  READY SCREEN
// ============================================================
void LCDDISPLAY::showReadyScreen(int minutesGranted, int coinsInserted) {
  tft->fillScreen(C_BLACK);

  drawHeader(C_GREEN, "Ready to Start!");

  tft->setTextColor(C_WHITE);
  tft->setTextSize(2);
  tft->setCursor(10, 64);
  tft->print("Paid: P");
  tft->print(coinsInserted);

  tft->setCursor(10, 92);
  tft->print("Time: ");
  printDuration(minutesGranted);

  // Big START prompt
  tft->setTextColor(C_GREEN);
  tft->setTextSize(3);
  tft->setCursor(40, 152);
  tft->print("Press");
  tft->setCursor(30, 190);
  tft->print("START");

  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 252);
  tft->print("[STOP] to cancel");
}

// ============================================================
//  CHARGING SCREEN  (static — countdown updated separately)
// ============================================================
void LCDDISPLAY::showChargingScreen(int minutesGranted) {
  tft->fillScreen(C_BLACK);

  // Build header line 2 with session duration
  char line2[22];
  if (minutesGranted >= 60) {
    int h = minutesGranted / 60;
    int m = minutesGranted % 60;
    if (m > 0) snprintf(line2, sizeof(line2), "%dh %dm session", h, m);
    else        snprintf(line2, sizeof(line2), "%d hr session",  h);
  } else {
    snprintf(line2, sizeof(line2), "%d min session", minutesGranted);
  }

  drawHeader(C_GREEN, "Charging...", line2);

  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 122);
  tft->print("Time Remaining:");

  // Green accent bar
  tft->fillRect(0, 220, 240, 4, C_GREEN);

  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 246);
  tft->print("[STOP] to end early");
}

// ============================================================
//  COUNTDOWN  (partial redraw — no screen flicker)
//  Auto-switches HH:MM:SS ↔ MM:SS depending on time left
// ============================================================
void LCDDISPLAY::updateCountdown(long chargingEndTime) {
  long msLeft = chargingEndTime - millis();
  if (msLeft < 0) msLeft = 0;

  int totalSecs = (int)(msLeft / 1000);
  int hrs       = totalSecs / 3600;
  int minsLeft  = (totalSecs % 3600) / 60;
  int secsLeft  = totalSecs % 60;

  // Erase countdown area only
  tft->fillRect(10, 134, 220, 70, C_BLACK);
  tft->setTextColor(C_GREEN);

  if (hrs > 0) {
    // HH:MM:SS  — size 3 to fit 240px width
    tft->setTextSize(3);
    tft->setCursor(14, 150);
    if (hrs < 10) tft->print("0");
    tft->print(hrs);
    tft->print(":");
    if (minsLeft < 10) tft->print("0");
    tft->print(minsLeft);
    tft->print(":");
    if (secsLeft < 10) tft->print("0");
    tft->print(secsLeft);
  } else {
    // MM:SS  — size 4, more readable for sub-hour
    tft->setTextSize(4);
    tft->setCursor(30, 146);
    if (minsLeft < 10) tft->print("0");
    tft->print(minsLeft);
    tft->print(":");
    if (secsLeft < 10) tft->print("0");
    tft->print(secsLeft);
  }
}

// ============================================================
//  DONE SCREEN
// ============================================================
void LCDDISPLAY::showDoneScreen(bool earlyStop) {
  tft->fillScreen(C_BLACK);

  if (earlyStop) {
    drawHeader(C_RED, "Charging Stopped");
  } else {
    drawHeader(C_GREEN, "Charge Done!");
  }

  tft->setTextColor(C_WHITE);
  tft->setTextSize(2);
  tft->setCursor(10, 72);
  tft->print("Thank you!");

  tft->setTextColor(C_YELLOW);
  tft->setCursor(10, 110);
  tft->print("Please unplug");
  tft->setCursor(10, 138);
  tft->print("your eBike.");

  tft->setTextColor(C_LTGRAY);
  tft->setTextSize(1);
  tft->setCursor(10, 278);
  tft->print("Restarting in 3 seconds...");
}

// ============================================================
//  ERROR SCREEN
// ============================================================
void LCDDISPLAY::showError(String msg) {
  tft->fillScreen(C_BLACK);
  drawHeader(C_RED, "Error");

  tft->setTextColor(C_WHITE);
  tft->setTextSize(2);
  tft->setCursor(10, 60);

  int nl = msg.indexOf('\n');
  if (nl >= 0) {
    tft->print(msg.substring(0, nl));
    tft->setCursor(10, 90);
    tft->print(msg.substring(nl + 1));
  } else {
    tft->print(msg);
  }
}

// ============================================================
//  NON-BLOCKING ANIMATION  —  HOME SCREEN
//
//  Icon: eBike lightning bolt / plug  cycling 6 frames
//  Draw zone: x=10,y=252  w=60,h=44   (erased each frame)
//  Interval : ANIM_HOME_MS ms  (600ms default)
//
//  Call tickHomeAnim() every loop() — returns instantly if
//  interval has not elapsed. Zero delay() anywhere.
//
//  Frames cycle through a simple plug-icon with animated
//  lightning bolts flickering in sequence to suggest "power":
//    0: bolt OFF   dim plug only
//    1: bolt A ON  (top spark)
//    2: bolt A+B   (two sparks)
//    3: bolt full  (bright glow)
//    4: bolt fades (medium)
//    5: bolt OFF   back to dim
// ============================================================
void LCDDISPLAY::resetHomeAnim() {
  _homeAnimFrame = 0;
  _homeAnimLast  = 0;
}

void LCDDISPLAY::tickHomeAnim() {
  unsigned long now = millis();
  if (now - _homeAnimLast < ANIM_HOME_MS) return;   // not yet — return immediately
  _homeAnimLast = now;
  _homeAnimFrame = (_homeAnimFrame + 1) % 6;
  drawHomeIcon(_homeAnimFrame);
}

void LCDDISPLAY::drawHomeIcon(uint8_t frame) {
  // Erase own zone only — no full screen redraw
  // Zone: x=10, y=252, w=64, h=44
  const int IX = 10, IY = 252, IW = 64, IH = 44;
  tft->fillRect(IX, IY, IW, IH, C_BLACK);

  // --- Plug body (static outline) ---
  // Plug rectangle body
  tft->drawRect(IX+4,  IY+12, 20, 26, C_LTGRAY);
  // Plug prongs (top)
  tft->drawFastVLine(IX+9,  IY+6,  8, C_LTGRAY);
  tft->drawFastVLine(IX+18, IY+6,  8, C_LTGRAY);
  // Plug cord (bottom)
  tft->drawFastVLine(IX+14, IY+38, 6, C_LTGRAY);

  // --- Lightning bolt (frame-driven brightness) ---
  // Bolt drawn right of plug at x=38..62, y=252..295
  // 4-point zigzag: top → mid-right → mid-left → bottom
  // Colour cycles from dim → bright → dim
  uint16_t boltColor;
  bool      showBolt = true;
  switch (frame) {
    case 0: showBolt = false;                  break;
    case 1: boltColor = C_DKGRAY;              break;
    case 2: boltColor = 0xFD00;  /* amber */   break;
    case 3: boltColor = C_YELLOW;              break;
    case 4: boltColor = 0xFD00;  /* amber */   break;
    case 5: boltColor = C_DKGRAY;              break;
    default: showBolt = false;                 break;
  }

  if (showBolt) {
    // Zigzag bolt: 5 points
    int bx = IX + 38;
    int by = IY;
    // Segment 1: top-center → mid-right
    tft->drawLine(bx+8,  by+2,  bx+18, by+18, boltColor);
    // Segment 2: mid-right → mid-left (the cross-stroke)
    tft->drawLine(bx+18, by+18, bx+4,  by+22, boltColor);
    // Segment 3: mid-left → bottom-center
    tft->drawLine(bx+4,  by+22, bx+14, by+40, boltColor);

    // Extra thickness on bright frames — draw again offset 1px
    if (frame == 3) {
      tft->drawLine(bx+9,  by+2,  bx+19, by+18, boltColor);
      tft->drawLine(bx+19, by+18, bx+5,  by+22, boltColor);
      tft->drawLine(bx+5,  by+22, bx+15, by+40, boltColor);
    }
  }

  // Tiny dots on prongs — glow when bolt is bright
  if (frame == 2 || frame == 3 || frame == 4) {
    uint16_t glow = (frame == 3) ? C_YELLOW : 0xFD00;
    tft->fillRect(IX+8,  IY+4, 3, 3, glow);
    tft->fillRect(IX+17, IY+4, 3, 3, glow);
  }
}

// ============================================================
//  NON-BLOCKING ANIMATION  —  COIN WAITING SCREEN
//
//  Icon: falling coin  (8-frame bounce)
//  Draw zone: x=165, y=218, w=65, h=34   (erased each frame)
//  Interval : ANIM_COIN_MS ms  (180ms default)
//
//  The coin slides down then bounces — simple circle at
//  8 Y positions to imply motion without physics.
//  Frame pattern:  0(top) → ... → 5(bottom) → ... → 7(bounce-up)
//  Coin colour alternates gold / bright to fake a spin flash.
//
//  IMPORTANT: This zone (x=165..230, y=218..252) must NOT be
//  touched by updateCoinWaiting() — they are kept separate so
//  the animation never flickers when coins are inserted.
// ============================================================
void LCDDISPLAY::resetCoinAnim() {
  _coinAnimFrame = 0;
  _coinAnimLast  = 0;
}

void LCDDISPLAY::tickCoinAnim() {
  unsigned long now = millis();
  if (now - _coinAnimLast < ANIM_COIN_MS) return;   // not yet
  _coinAnimLast = now;
  _coinAnimFrame = (_coinAnimFrame + 1) % 8;
  drawCoinIcon(_coinAnimFrame);
}

void LCDDISPLAY::drawCoinIcon(uint8_t frame) {
  // Erase own zone only
  // Zone: x=165, y=218, w=65, h=34
  const int CX = 165, CY = 218, CW = 65, CH = 34;
  tft->fillRect(CX, CY, CW, CH, C_BLACK);

  // Coin Y positions for 8 frames (drop then bounce):
  // 0=top of zone, slide to bottom, quick bounce back
  //   frame:    0    1    2    3    4    5    6    7
  const int8_t yOff[] = { 0,  4,  8, 14, 18, 22, 14,  6 };
  //  coin spin: alternate gold / bright to imply rotation
  const bool   spin[]  = { false, false, true, false, true, false, true, false };

  int cy = CY + yOff[frame % 8];
  int cx = CX + 30;  // horizontal centre
  int r  = 10;       // coin radius

  uint16_t coinColor  = spin[frame % 8] ? C_WHITE   : C_YELLOW;
  uint16_t innerColor = spin[frame % 8] ? 0xFD20    : 0xFD00;
  uint16_t rimColor   = spin[frame % 8] ? C_YELLOW  : 0xFD00;

  // Coin body
  tft->fillCircle(cx, cy + r, r,     coinColor);
  tft->fillCircle(cx, cy + r, r - 3, innerColor);

  // Rim
  tft->drawCircle(cx, cy + r, r,     rimColor);

  // "P" symbol inside coin (tiny, size 1)
  tft->setTextColor(C_BLACK);
  tft->setTextSize(1);
  tft->setCursor(cx - 3, cy + r - 3);
  tft->print("P");

  // Motion trail — ghost above coin on fast frames (1-4)
  if (frame >= 1 && frame <= 4) {
    int trailY = cy - 6;
    if (trailY >= CY) {
      tft->drawCircle(cx, trailY + r, r - 4, C_DKGRAY);
    }
  }

  // Small bounce lines at bottom when coin lands (frame 4-5)
  if (frame == 4 || frame == 5) {
    int landY = CY + 26;
    tft->drawFastHLine(cx - 8, landY, 4, C_DKGRAY);
    tft->drawFastHLine(cx + 4, landY, 4, C_DKGRAY);
  }
}