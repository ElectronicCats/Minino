#include <ezButton.h>

#include "Arduino.h"
#include "Pins.h"

#define DEBOUNCE_DELAY_MS 50

class Keyboard {
 public:
  Keyboard();
  void loop();
  ezButton up;
  ezButton down;
  ezButton right;
  ezButton left;
  ezButton select;
  void printPressedButton();
};