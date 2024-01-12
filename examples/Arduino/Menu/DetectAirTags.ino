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

void toggleList(bool isBlackList, int selectedAirTag) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  drawFrame();

  if (isBlackList) {
    focusBlackTitle();
    int count = 0;

    for (auto &address : airTags) {
      int index = std::distance(airTags.begin(), std::find(airTags.begin(), airTags.end(), address));
      String message = " " + String(index) + " " + address + " ";

      if (count < 5) {
        count++;
        if (index == selectedAirTag) {
          display.setTextColor(BLACK, WHITE);
          display.println(message);
        } else {
          display.setTextColor(WHITE);
          display.println(message);
        }
      }
    }
  } else {
    focusWhiteTitle();
  }
  display.display();
}

void detectAirTags() {
  toggleList(true, 0);

  while (true) {
    static bool isBlackList = true;
    static int selectedAirTag = 0;

    // Exit to main menu
    keyboard.loop();
    if (keyboard.left.isPressed()) {
      isBlackList = true;
      break;
    }

    // Toggle list with right button
    if (keyboard.right.isPressed()) {
      isBlackList = !isBlackList;
      selectedAirTag = 0;
      toggleList(isBlackList, selectedAirTag);
    }

    // Select AirTag with up and down buttons
    if (keyboard.up.isPressed()) {
      selectedAirTag = constrain(selectedAirTag - 1, 0, airTags.size() - 1);
      Serial.println(selectedAirTag);
      toggleList(isBlackList, selectedAirTag);
    }

    if (keyboard.down.isPressed()) {
      selectedAirTag = constrain(selectedAirTag + 1, 0, airTags.size() - 1);
      Serial.println(selectedAirTag);
      toggleList(isBlackList, selectedAirTag);
    }
  }
}
