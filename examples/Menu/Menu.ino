#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>
#include <ArduinoBLE.h>
#include <vector>

#include "Keyboard.h"
#include "Layer.h"
#include "enum.h"
#include "DetectAirTags.h"

#define BLACK SH110X_BLACK
#define WHITE SH110X_WHITE

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Layer layer;
int selectedOption = layer.Scan;  // Default selected option
int currentLayer = layer.Menu;    // Default layer

// Detect AirTags variables
uint8_t leds[] = {18, 19, 22, 21};  // Frontier board - Bast
uint8_t advertisement[31];          // Save actual AirTag advertising packet

// Sound variables
uint8_t limitDevices = 10;        // How many devices able to detect (if this variable change, must change the next variables section)
unsigned long delayStart[10];     // Activity per AirTag
bool actives[10];                 // To check if the Airtag is around more than 10 minutes
String mairtags[10];              // Save Mac AirTags for reference
// uint8_t advertisement[31];        // Save actual AirTag Advertising
uint8_t advertags[10][31];        // Save active AirTags Advertinsings
uint8_t positiona = 0;            // The last Active AirTag
bool playSoundm = false;          // Write chrac to play a sound without waiting 10 minutes
float MINUTES = 10.2;             // Minimum 10 minutes to make it play a sound
unsigned long ONEMINUTE = 60000;  // 1 minute
unsigned long DELAY_TIME = ONEMINUTE * MINUTES;
uint8_t delayRunning = 1;  // True if still waiting for a delay to finish

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
  setupSound();
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
      spoof();
      currentLayer = layer.Menu;
      break;
    case layer.Detect:
      detectAirTags();
      currentLayer = layer.Menu;
      break;
    case layer.Sound:
      sound();
      currentLayer = layer.Menu;
      break;
  }
  display.display();
}

void displayMenu(void) {
  keyboard.loop();

  if (keyboard.up.isPressed()) {
    selectedOption = selectedOption - 1;  // Move the selection up
  }

  if (keyboard.down.isPressed()) {
    selectedOption = selectedOption + 1;  // Move the selection down
  }

  if (keyboard.select.isPressed()) {
    currentLayer = selectedOption;  // Select the current layer
  }

  if (selectedOption < layer.Scan) {
    selectedOption = layer.Sound;  // Go to the last option
  }

  if (selectedOption > layer.Sound) {
    selectedOption = layer.Scan;  // Go to the first option
  }

  showLayer();
}
