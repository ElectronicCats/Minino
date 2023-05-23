#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <ezButton.h>

#include "enum.h"

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
uint8_t advertisement[31];

int selected = 0;
int Layer = -1;

void setup() {
  Serial.begin(9600);

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
}

void loop() {
  // displayMenu();
}

void displayMenu(void) {
  int down = digitalRead(6);
  int up = digitalRead(5);
  int enter = digitalRead(17);
  int back = digitalRead(2);

  if (up == LOW && down == LOW) {
  };
  if (up == LOW) {
    selected = selected + 1;
    delay(200);
  };
  if (down == LOW) {
    selected = selected - 1;
    delay(200);
  };
  if (enter == LOW) {
    Layer = selected;
  };
  if (back == LOW) {
    Layer = -1;
  };
  const char *options[4] = {
      " 1.-SCAN",
      " 2.-SPOOF",
      " 3.-DETECT",
      " 4.-SOUND "
      };

  if (Layer == -1) {
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
  } else if (Layer == 0) {
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
  } else if (Layer == 1) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Spoof"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Spoofing...");
  } else if (Layer == 2) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Detect"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(2);
    display.println("Detecting...");
  } else if (Layer == 3) {
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
