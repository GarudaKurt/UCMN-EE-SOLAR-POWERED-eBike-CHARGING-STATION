#ifndef __KEYPADMENU__H
#define __KEYPADMENU__H

#include <Keypad.h>

class KEYPADMENU {
  public:
    KEYPADMENU();
    ~KEYPADMENU();
    char getInputs();
    void clearLastKey();

  private:
    static const byte ROWS = 4;
    static const byte COLS = 4;
    char keys[ROWS][COLS] = {
      {'1', '4', '7', '*'},
      {'2', '5', '8', '0'},
      {'3', '6', '9', '#'},
      {'A', 'B', 'C', 'D'}
    };
    byte rowPins[ROWS] = {31, 32, 33, 34};
    byte colPins[COLS] = {35, 36, 37, 38};
    Keypad* keypad;  // Pointer
};

#endif