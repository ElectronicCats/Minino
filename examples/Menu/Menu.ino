#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>

#include "Keyboard.h"
#include "enum.h"
#include "Layer.h"

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// uint8_t advertisement[31];

Layer layer;
int selected = layer.Scan;
int currentLayer = layer.Menu;

Keyboard keyboard;

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");

  //  inicialización
  if (!display.begin(SCREEN_ADDRESS)) {
    Serial.println(F("SH110X allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("BIENVENIDO");
  display.display();

  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  display.display();
  display.drawBitmap((display.width() - LOGO_WIDTH) / 2, ((display.height() - LOGO_HEIGHT) / 2) + 4, logo, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
  display.display();
  delay(2000);

  setupBLE();
}

void loop() {
  keyboard.loop();
  keyboard.printPressedButton();
  displayMenu();
}

void displayMenu(void) {
  if (keyboard.up.isPressed() && selected > 0) {
    selected = selected - 1;
  }
  if (keyboard.down.isPressed() && selected < 3) {
    selected = selected + 1;
  }
  if (keyboard.select.isPressed()) {
    currentLayer = selected;
  }
  if (keyboard.left.isPressed()) {
    currentLayer = layer.Menu;
  }

  const char *options[4] = {
      " 1. SCAN",
      " 2. SPOOF",
      " 3. DETECT",
      " 4. SOUND "};

  if (currentLayer == layer.Menu) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("MAIN MENU"));
    display.print(selected);
    display.println("");
    for (int i = 0; i < 4; i++) {
      if (i == selected) {
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.println(options[i]);
      } else if (i != selected) {
        display.setTextColor(SH110X_WHITE);
        display.println(options[i]);
      }
    }
  } else if (currentLayer == layer.Scan) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Scan"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Scanning...");
    // llamar funcion de scan
    // una vez presionado back sale de funcion
  } else if (currentLayer == layer.Spoof) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Spoof"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Spoofing...");
  } else if (currentLayer == layer.Detect) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Detect"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Detecting...");
  } else if (currentLayer == layer.Sound) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Sound"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Sound...");
  }
  display.display();
}
