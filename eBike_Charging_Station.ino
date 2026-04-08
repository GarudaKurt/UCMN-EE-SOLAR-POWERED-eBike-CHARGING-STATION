// ============================================================
//  eBike Solar Charging Station  —  Main
//  Hardware : Arduino Mega 2560
//
//  PRICING  (all defined in lcdDisplay.h — change once there)
//    1 peso  =  3 minutes  (linear, no blocks)
//    P1  =   3 min
//    P5  =  15 min
//    P10 =  30 min
//    P20 =  60 min
//
//  Two modes of operation:
//  1. KEYPAD ENTRY  — user types target minutes → system shows
//                     required pesos → user inserts coins to meet it
//  2. SHORTCUT KEYS — A=15min / B=30min / C=60min
//
//  Coin screen is LIVE — time displayed grows with every peso inserted
//  Charging uses minutesGranted = coinsInserted * MINS_PER_PESO
// ============================================================

#include <Arduino.h>
#include "lcdDisplay.h"
#include "KeypadMenu.h"

// -----------------------------------------------------------
//  PIN DEFINITIONS
// -----------------------------------------------------------
#define COIN_PIN   2    // Allan Universal coin slot — interrupt pin
#define SSR_PIN    8    // SSR-40DA control (HIGH = relay ON)
#define BTN_START 31    // START push button (INPUT_PULLUP, LOW = pressed)
#define BTN_STOP  32    // STOP  push button (INPUT_PULLUP, LOW = pressed)

#define PULSES_PER_PESO  1   // Allan Universal: 1 pulse = P1

// -----------------------------------------------------------
//  OBJECTS
// -----------------------------------------------------------
LCDDISPLAY  lcd;
KEYPADMENU  keypadMenu;

// -----------------------------------------------------------
//  STATE MACHINE
// -----------------------------------------------------------
enum State {
  STATE_IDLE,           // Welcome screen, waiting for first input
  STATE_SELECT,         // User typing custom minutes on keypad
  STATE_AWAITING_COIN,  // Waiting for coins to reach requiredPesos
  STATE_READY,          // Coins sufficient, waiting for START press
  STATE_CHARGING,       // Relay ON, countdown running
  STATE_DONE            // Session finished — auto-resets after 3s
};
State currentState = STATE_IDLE;

// -----------------------------------------------------------
//  SESSION VARIABLES
// -----------------------------------------------------------
volatile int coinPulseCount = 0;   // Written ONLY inside ISR

int  coinsInserted    = 0;   // Total pesos received this session
int  requiredPesos    = 0;   // Pesos needed for selected minutes
int  minutesGranted   = 0;   // coinsInserted * MINS_PER_PESO (live)
long chargingEndTime  = 0;   // millis() target when session ends
long lastDisplayUpdate = 0;  // throttle display refreshes
String inputBuffer    = "";  // raw digits typed on keypad

// -----------------------------------------------------------
//  COIN SLOT ISR  —  RISING edge = 1 pulse = P1
// -----------------------------------------------------------
void coinISR() {
  coinPulseCount++;
}

// -----------------------------------------------------------
//  SETUP
// -----------------------------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(SSR_PIN,   OUTPUT);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_STOP,  INPUT_PULLUP);
  pinMode(COIN_PIN,  INPUT_PULLUP);
  digitalWrite(SSR_PIN, LOW);   // Relay OFF — safety at startup

  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinISR, RISING);

  lcd.begin();
  lcd.showWelcome();
}

// ============================================================
//  SESSION CONTROL
// ============================================================

// Start coin-waiting for a given target minute count
void beginSession(int targetMinutes) {
  requiredPesos     = LCDDISPLAY::minutesToPesos(targetMinutes);
  coinsInserted     = 0;
  minutesGranted    = 0;
  inputBuffer       = "";
  lastDisplayUpdate = millis();
  currentState      = STATE_AWAITING_COIN;

  // ✅ showCoinWaiting called ONCE here — draws full static layout
  lcd.showCoinWaiting(requiredPesos, coinsInserted, minutesGranted);
  lcd.resetCoinAnim();

  Serial.print(F("[SESSION] target="));
  Serial.print(targetMinutes);
  Serial.print(F("min  required=P"));
  Serial.println(requiredPesos);
}

// Called whenever new coin pulses arrive
// Every single peso immediately adds MINS_PER_PESO to granted time
void handleCoins() {
  if (currentState != STATE_AWAITING_COIN) return;

  // Linear — no blocks, no rounding
  minutesGranted = LCDDISPLAY::pesosToMinutes(coinsInserted);

  Serial.print(F("[COIN] P"));
  Serial.print(coinsInserted);
  Serial.print(F(" -> "));
  Serial.print(minutesGranted);
  Serial.println(F(" min"));

  if (coinsInserted >= requiredPesos) {
    currentState = STATE_READY;
    lcd.showReadyScreen(minutesGranted, coinsInserted);
  } else {
    // ✅ updateCoinWaiting — partial redraw only (progress bar +
    //    inserted/need values + time granted). Header, labels,
    //    hint text, and animation zone are never touched.
    lcd.updateCoinWaiting(requiredPesos, coinsInserted, minutesGranted);
  }
}

void startCharging() {
  // Session duration = exactly what the coins bought
  chargingEndTime   = millis() + ((long)minutesGranted * 60000L);
  lastDisplayUpdate = millis();
  currentState      = STATE_CHARGING;
  digitalWrite(SSR_PIN, HIGH);   // SSR-40DA ON → AC flows to eBike

  lcd.showChargingScreen(minutesGranted);

  Serial.print(F("[CHARGING] "));
  Serial.print(minutesGranted);
  Serial.println(F(" min started"));
}

void stopCharging(bool earlyStop) {
  digitalWrite(SSR_PIN, LOW);    // SSR-40DA OFF immediately
  currentState = STATE_DONE;
  lcd.showDoneScreen(earlyStop);

  Serial.print(F("[STOP] early="));
  Serial.println(earlyStop ? F("YES") : F("NO"));
}

void cancelSession() {
  digitalWrite(SSR_PIN, LOW);
  resetSession();
}

void resetSession() {
  coinsInserted     = 0;
  requiredPesos     = 0;
  minutesGranted    = 0;
  inputBuffer       = "";
  coinPulseCount    = 0;
  lastDisplayUpdate = 0;
  currentState      = STATE_IDLE;
  lcd.showWelcome();
  lcd.resetHomeAnim();
}

// ============================================================
//  KEYPAD HANDLERS
// ============================================================

// IDLE — first keypress decides mode
void handleKeyIdle(char key) {
  if (key >= '1' && key <= '9') {
    currentState = STATE_SELECT;
    inputBuffer  = String(key);
    lcd.showSelectScreen(inputBuffer, inputBuffer.toInt());
  } else if (key == 'A') { beginSession(SHORTCUT_A_MINS); }
  else if   (key == 'B') { beginSession(SHORTCUT_B_MINS); }
  else if   (key == 'C') { beginSession(SHORTCUT_C_MINS); }
}

void handleKeySelect(char key) {
  if (key >= '0' && key <= '9') {
    if (!(inputBuffer.length() == 0 && key == '0') &&
         inputBuffer.length() < 3) {
      inputBuffer += key;
    }
    int preview = inputBuffer.toInt();
    lcd.showSelectScreen(inputBuffer, preview);

  } else if (key == '#') {
    int mins = inputBuffer.toInt();

    if (mins < MIN_MINUTES) {
      lcd.showError("Min 3 min\n(at least P1)");
      delay(1800);
      inputBuffer = "";
      lcd.showSelectScreen(inputBuffer, 0);

    } else if (mins > MAX_MINUTES) {
      String err = "Max ";
      err += MAX_MINUTES;
      err += " min";
      lcd.showError("Too long!\n" + err);
      delay(1800);
      inputBuffer = "";
      lcd.showSelectScreen(inputBuffer, 0);

    } else {
      beginSession(mins);
    }

  } else if (key == '*') {
    if (inputBuffer.length() > 0) {
      inputBuffer.remove(inputBuffer.length() - 1);
      int preview = (inputBuffer.length() > 0) ? inputBuffer.toInt() : 0;
      lcd.showSelectScreen(inputBuffer, preview);
    } else {
      currentState = STATE_IDLE;
      lcd.showWelcome();
    }

  } else if (key == 'A') { beginSession(SHORTCUT_A_MINS); }
  else if   (key == 'B') { beginSession(SHORTCUT_B_MINS); }
  else if   (key == 'C') { beginSession(SHORTCUT_C_MINS); }
}

// ============================================================
//  MAIN LOOP
// ============================================================
void loop() {
  char key = keypadMenu.getInputs();

  noInterrupts();
  int pulses = coinPulseCount;
  coinPulseCount = 0;
  interrupts();

  if (pulses > 0) {
    coinsInserted += pulses / PULSES_PER_PESO;
    handleCoins();
  }

  bool startPressed = (digitalRead(BTN_START) == LOW);
  bool stopPressed  = (digitalRead(BTN_STOP)  == LOW);

  switch (currentState) {

    case STATE_IDLE:
      if (key) handleKeyIdle(key);
      lcd.tickHomeAnim();          // non-blocking icon tick
      break;

    case STATE_SELECT:
      if (key) handleKeySelect(key);
      break;

    case STATE_AWAITING_COIN:
      lcd.tickCoinAnim();          // non-blocking coin drop tick
      if (stopPressed) cancelSession();
      break;

    case STATE_READY:
      if (startPressed) startCharging();
      if (stopPressed)  cancelSession();
      break;

    case STATE_CHARGING:
      if (stopPressed) {
        stopCharging(true);
        break;
      }
      if (millis() - lastDisplayUpdate > 1000) {
        lcd.updateCountdown(chargingEndTime);
        lastDisplayUpdate = millis();
      }
      if (millis() >= chargingEndTime) {
        stopCharging(false);
      }
      break;

    case STATE_DONE:
      delay(3000);
      resetSession();
      break;
  }

  delay(50);
}
