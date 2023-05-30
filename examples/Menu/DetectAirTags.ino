// #include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q

  LEDs detection - Apple AirTags
  Aug 10th, 2022
*/

void toggleList() {
  static bool isBlackList = true;

  if (isBlackList) {
    isBlackList = false;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(0, 0);
    display.print(F("Black list"));
    display.setTextColor(WHITE);
    display.println(F(" White list"));
    display.display();
  } else {
    isBlackList = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("Black list "));
    display.setTextColor(BLACK, WHITE);
    display.println(F("White list"));
    display.display();
  }
}

void detectAirTags() {
  toggleList();

  while (true) {
    static unsigned long lastTime = millis();

    // Exit to main menu
    keyboard.loop();
    if (keyboard.left.isPressed()) {
      break;
    }

    // Toggle list with right button
    if (keyboard.right.isPressed()) {
      toggleList();
    }
  }
}
