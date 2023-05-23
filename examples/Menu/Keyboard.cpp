#include "Keyboard.h"

Keyboard::Keyboard() : up(UP_PIN, DEBOUNCE_DELAY_MS),
                                           down(DOWN_PIN, DEBOUNCE_DELAY_MS),
                                           right(RIGHT_PIN, DEBOUNCE_DELAY_MS),
                                           left(LEFT_PIN, DEBOUNCE_DELAY_MS),
                                           select(SELECT_PIN, DEBOUNCE_DELAY_MS) {
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);

  this->up.setDebounceTime(DEBOUNCE_DELAY_MS);
  this->down.setDebounceTime(DEBOUNCE_DELAY_MS);
  this->right.setDebounceTime(DEBOUNCE_DELAY_MS);
  this->left.setDebounceTime(DEBOUNCE_DELAY_MS);
  this->select.setDebounceTime(DEBOUNCE_DELAY_MS);
}

/// @brief Polls the buttons for changes.
/// @details This method must be called in the loop where the buttons are used.
/// @note This method must be called before any of the other methods.
/// @return void
void Keyboard::loop() {
  this->up.loop();
  this->down.loop();
  this->right.loop();
  this->left.loop();
  this->select.loop();
}

void Keyboard::printPressedButton() {
  if (this->up.isPressed()) {
    Serial.println("UP");
  } else if (this->down.isPressed()) {
    Serial.println("DOWN");
  } else if (this->right.isPressed()) {
    Serial.println("RIGHT");
  } else if (this->left.isPressed()) {
    Serial.println("LEFT");
  } else if (this->left.isPressed()) {
    Serial.println("LEFT");
  } else if (this->select.isPressed()) {
    Serial.println("SELECT");
  }
}