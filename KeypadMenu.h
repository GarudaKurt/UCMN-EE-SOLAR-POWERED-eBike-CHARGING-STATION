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
      {'1', '2', '3', 'A'},
      {'4', '5', '6', 'B'},
      {'7', '8', '9', 'C'},
      {'*', '0', '#', 'D'}
    };
    byte rowPins[ROWS] = {25, 24, 23, 22};
    byte colPins[COLS] = {29, 28, 27, 26};
    Keypad* keypad;  // Pointer
};

#endif