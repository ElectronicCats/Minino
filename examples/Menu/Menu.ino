#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>

#include "Keyboard.h"
#include "Layer.h"
#include "enum.h"

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Layer layer;
int selectedOption = layer.Scan;  // Default selected option
int currentLayer = layer.Menu;    // Default layer

Keyboard keyboard;

void setup() {
  Serial.begin(9600);

  /// Start I2C communication
  if (!display.begin(SCREEN_ADDRESS)) {
    Serial.println(F("SH110X allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("WELCOME!");
  display.display();

  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  display.display();
  display.drawBitmap((display.width() - LOGO_WIDTH) / 2, ((display.height() - LOGO_HEIGHT) / 2) + 4, logo, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
  display.display();
  delay(1000);

  setupBLE();
}

void loop() {
  displayMenu();
}

void printOptions() {
  const char *options[4] = {
      " 1. SCAN",
      " 2. SPOOF",
      " 3. DETECT",
      " 4. SOUND "};

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("MAIN MENU"));
  display.println("");

  for (int i = 0; i < 4; i++) {
    if (i == selectedOption) {
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
      display.println(options[i]);
    } else if (i != selectedOption) {
      display.setTextColor(SH110X_WHITE);
      display.println(options[i]);
    }
  }
}

void showLayer() {
  switch (currentLayer) {
    case layer.Menu:
      printOptions();
      break;
    case layer.Scan:
      scanAirTags();
      currentLayer = layer.Menu;
      break;
    case layer.Spoof:
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 0);
      display.println(F("Spoof"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Spoofing...");
      break;
    case layer.Detect:
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 0);
      display.println(F("Detect"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Detecting...");
      break;
    case layer.Sound:
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 0);
      display.println(F("Sound"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Playing...");
      break;
  }
  display.display();
}

void displayMenu(void) {
  keyboard.loop();

  if (keyboard.up.isPressed() && selectedOption > layer.Scan) {
    selectedOption = selectedOption - 1;  // Move the selection up
  }
  if (keyboard.down.isPressed() && selectedOption < layer.Sound) {
    selectedOption = selectedOption + 1;  // Move the selection down
  }
  if (keyboard.select.isPressed()) {
    currentLayer = selectedOption;  // Select the current layer
  }

  if (keyboard.left.isPressed()) {
    currentLayer = layer.Menu;  // Go back to the main menu
  }

  showLayer();
}
