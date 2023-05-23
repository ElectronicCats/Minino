#include <ezButton.h>

#include "Arduino.h"

#define DEBOUNCE_DELAY_MS 50
#define UP_PIN 5
#define DOWN_PIN 6
#define RIGHT_PIN 10
#define LEFT_PIN 9
#define SELECT_PIN 46

class Keyboard {
 public:
  Keyboard();
  void loop();
  ezButton up;
  ezButton down;
  ezButton right;
  ezButton left;
  ezButton select;
};