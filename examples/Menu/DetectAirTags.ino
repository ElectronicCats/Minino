void drawFrame() {
  display.drawLine(0, 10, 128, 10, SH110X_WHITE);    // Top horizontal line
  display.drawLine(0, 10, 0, 64, SH110X_WHITE);      // Left vertical line
  display.drawLine(0, 63, 128, 63, SH110X_WHITE);    // Bottom horizontal line
  display.drawLine(127, 10, 127, 63, SH110X_WHITE);  // Right vertical line
}

void focusBlackTitle() {
  display.setTextColor(BLACK, WHITE);
  display.print(F("Black list"));
  display.setTextColor(WHITE);
  display.println(F(" White list"));
  display.println("");
}

void focusWhiteTitle() {
  display.setTextColor(WHITE);
  display.print(F("Black list "));
  display.setTextColor(BLACK, WHITE);
  display.println(F("White list"));
}

void toggleList(bool isBlackList) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  drawFrame();

  if (isBlackList) {
    focusBlackTitle();

    for (auto &address : airTags) {
      int index = std::distance(airTags.begin(), std::find(airTags.begin(), airTags.end(), address));
      String message = " " + String(index) + " " + address;
      display.println(message);
    }
  } else {
    focusWhiteTitle();
  }
  display.display();
}

void detectAirTags() {
  toggleList(true);

  while (true) {
    static bool isBlackList = false;

    // Exit to main menu
    keyboard.loop();
    if (keyboard.left.isPressed()) {
      isBlackList = false;
      break;
    }

    // Toggle list with right button
    if (keyboard.right.isPressed()) {
      toggleList(isBlackList);
      isBlackList = !isBlackList;
    }
  }
}
