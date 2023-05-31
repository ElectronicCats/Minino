void drawFrame() {
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);    // Top horizontal line
  display.drawLine(0, 10, 0, 64, SH110X_WHITE);      // Left vertical line
  display.drawLine(0, 63, 128, 63, SH110X_WHITE);    // Bottom horizontal line
  display.drawLine(127, 10, 127, 63, SH110X_WHITE);  // Right vertical line
}

void toggleList() {
  static bool isBlackList = true;
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  drawFrame();

  if (isBlackList) {
    isBlackList = false;
    display.setTextColor(BLACK, WHITE);
    display.print(F("Black list"));
    display.setTextColor(WHITE);
    display.println(F(" White list"));
    display.println("");

    for (auto &address : airTags) {
      String message = "  " + address;
      display.println(message);
    }
  } else {
    isBlackList = true;
    display.setTextColor(WHITE);
    display.print(F("Black list "));
    display.setTextColor(BLACK, WHITE);
    display.println(F("White list"));
  }
  display.display();
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
