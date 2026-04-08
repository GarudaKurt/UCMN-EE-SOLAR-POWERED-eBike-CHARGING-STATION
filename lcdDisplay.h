#ifndef __LCDDISPLAY__H
#define __LCDDISPLAY__H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Arduino.h>

// -----------------------------------------------------------
//  TFT PIN DEFINITIONS
// -----------------------------------------------------------
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST  12

// TFT SDA (MOSI) = Pin 11  (hardware SPI)
// TFT SCK        = Pin 13  (hardware SPI)

// -----------------------------------------------------------
//  PRICING MODEL  —  strictly linear: 1 peso = MINS_PER_PESO mins
//
//  Coin insertion  →  time is granted per-peso continuously
//    P1  =   3 min
//    P5  =  15 min   (5 × 3)
//    P7  =  21 min   (7 × 3)
//    P10 =  30 min   (10 × 3)
//    P20 =  60 min   (20 × 3)
//    P40 = 120 min   (40 × 3)
//    P65 = 195 min   (65 × 3  =  3h 15m)
//
//  Keypad entry  →  user types MINUTES
//    System shows  ceil(minutes / MINS_PER_PESO)  pesos required
//    e.g.  15 min  →  ceil(15/3) = P5
//          25 min  →  ceil(25/3) = P9  (8.33 → 9)
//          60 min  →  ceil(60/3) = P20
//
//  Minimum session: MIN_MINUTES (= 1 peso worth)
//  Maximum session: MAX_MINUTES
// -----------------------------------------------------------
#define MINS_PER_PESO    3      // 1 peso = 3 minutes
#define MIN_MINUTES      3      // minimum = P1 = 3 min
#define MAX_MINUTES    300      // maximum = P100 = 300 min (5 hrs)

// -----------------------------------------------------------
//  SHORTCUT KEYS  (A / B / C)  — in minutes
// -----------------------------------------------------------
#define SHORTCUT_A_MINS   15   // A  →  P5
#define SHORTCUT_B_MINS   30   // B  →  P10
#define SHORTCUT_C_MINS   60   // C  →  P20

// -----------------------------------------------------------
//  COLOURS  (RGB565)
// -----------------------------------------------------------
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_YELLOW  0xFFE0
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_CYAN    0x07FF
#define C_ORANGE  0xFD20
#define C_DKGRAY  0x4208
#define C_LTGRAY  0xC618
#define C_BLUE    0x001F

class LCDDISPLAY {
  public:
    LCDDISPLAY();
    void begin();

    // Screens
    void showWelcome();
    void showSelectScreen(String inputBuffer, int previewMinutes);
    void showCoinWaiting(int requiredPesos, int coinsInserted, int minutesGranted);
    void updateCoinWaiting(int requiredPesos, int coinsInserted, int minutesGranted); // <-- NEW
    void showReadyScreen(int minutesGranted, int coinsInserted);
    void showChargingScreen(int minutesGranted);
    void showDoneScreen(bool earlyStop);
    void showError(String msg);

    // Partial redraw during countdown
    void updateCountdown(long chargingEndTime);

    // -----------------------------------------------------------
    //  Non-blocking animation ticks
    //  Call these every loop() iteration for the matching screen.
    //  Each call returns immediately if its interval hasn't elapsed.
    //  ANIM_INTERVAL_HOME_MS  — welcome screen icon cycle
    //  ANIM_INTERVAL_COIN_MS  — coin drop icon bounce
    // -----------------------------------------------------------
    void tickHomeAnim();
    void tickCoinAnim();

    // Reset animation state when entering a screen
    void resetHomeAnim();
    void resetCoinAnim();

    // -----------------------------------------------------------
    //  Static pricing utilities  (callable from main without object)
    // -----------------------------------------------------------

    // Minutes  →  pesos required   ceil(minutes / MINS_PER_PESO)
    static int minutesToPesos(int minutes);

    // Pesos inserted  →  minutes granted   pesos * MINS_PER_PESO
    // Every peso counts — no rounding, no blocks
    static int pesosToMinutes(int pesos);

    // Validate a minute value entered on keypad
    static bool isValidMinutes(int minutes);

  private:
    Adafruit_ST7789* tft;

    // -----------------------------------------------------------
    //  Animation state  — all millis()-based, zero delay()
    // -----------------------------------------------------------

    // Home screen: rotating lightning bolt / plug icon cycle
    // Zone: bottom area 0,255 → 240,320  (below footer line)
    // Actually drawn in a reserved icon strip y=250..298
    unsigned long _homeAnimLast  = 0;
    uint8_t       _homeAnimFrame = 0;      // 0..5 cycling frames
    static const uint16_t ANIM_HOME_MS = 600;   // frame interval ms

    // Coin screen: bouncing coin drop icon
    // Zone: right side x=155,y=218 → 240,252  (beside granted time)
    unsigned long _coinAnimLast  = 0;
    uint8_t       _coinAnimFrame = 0;      // 0..7  (8-frame bounce)
    static const uint16_t ANIM_COIN_MS = 180;   // frame interval ms

    // Internal helpers
    void drawHeader(uint16_t color, const char* line1, const char* line2 = nullptr);
    void drawProgressBar(int x, int y, int w, int h,
                         int value, int maxVal, uint16_t fillColor);
    void printDuration(int totalMinutes);

    // Icon primitives  (all partial-redraw safe — erase own zone first)
    void drawHomeIcon(uint8_t frame);
    void drawCoinIcon(uint8_t frame);
};

#endif