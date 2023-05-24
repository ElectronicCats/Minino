#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>

#include "Keyboard.h"
#include "Layer.h"
#include "enum.h"

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// uint8_t advertisement[31];

Layer layer;
int selected = layer.Scan;
int currentLayer = layer.Menu;
bool detectAirTagsFlag = false;

const char *options[4] = {
    " 1. SCAN",
    " 2. SPOOF",
    " 3. DETECT",
    " 4. SOUND "};

Keyboard keyboard;

void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;
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
  displayMenu();

  if (detectAirTagsFlag) detectAirTags();
}

void printOptions() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("MAIN MENU"));
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
}

void showLayer(int currentLayer) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  switch (currentLayer) {
    case layer.Menu:
      printOptions();
      break;
    case layer.Scan:
      display.println(F("Scan"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Scanning...");
      detectAirTagsFlag = true;
      break;
    case layer.Spoof:
      display.println(F("Spoof"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Spoofing...");
      break;
    case layer.Detect:
      display.println(F("Detect"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Detecting...");
      break;
    case layer.Sound:
      display.println(F("Sound"));
      display.println("configuration");
      display.setTextSize(2);
      display.println("Playing...");
      break;
  }
  display.display();
}

void displayMenu(void) {
  if (keyboard.up.isPressed() && selected > layer.Scan) {
    selected = selected - 1;
  }
  if (keyboard.down.isPressed() && selected < layer.Sound) {
    selected = selected + 1;
  }
  if (keyboard.select.isPressed()) {
    currentLayer = selected;
  }
  if (keyboard.left.isPressed()) {
    currentLayer = layer.Menu;
    detectAirTagsFlag = false;
  }

  showLayer(currentLayer);
}
