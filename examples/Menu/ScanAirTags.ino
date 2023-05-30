// #include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q

  LEDs detection - Apple AirTags
  Aug 10th, 2022
*/

#define SCAN_DELAY_MS 500
#define SHOW_DISTANCE_DELAY_MS 2000

void setupBLE() {
  Serial.begin(9600);

  for (uint8_t i = 0; i < 4; i++) {
    // pinMode(leds[i], OUTPUT);
    Serial.println(leds[i]);
  }

  // Begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1)
      ;
  }

  Serial.println("BLE Central scan");

  // start scanning for peripheral
  BLE.scan();
}

void ledon(int from, int to) {
  // for (int i = from; i < to; i++){
  //   digitalWrite(leds[i], HIGH);
  // }
}

void ledoff(int from, int to) {
  // for (int i = from; i < to; i++){
  //   digitalWrite(leds[i], LOW);
  // }
}

void rssip(int rssid) {  // Check the signal power to know how far the AirTag is located
  Serial.println();

  Serial.print("RSSI: ");
  Serial.println(rssid);
  Serial.println();
  Serial.print("Distance: ");

  if (rssid > -30) {
    Serial.println("Very close");
    display.println("Distance: ");
    display.println("Very close");
    for (int x = 0; x < 4; x++) {
      ledoff(0, 4);
      delay(20);
      ledon(0, 4);
    }
  }

  else if (rssid <= -30 && rssid > -60) {
    Serial.println("Near");
    display.print("Distance: ");
    display.println("Near");
    ledoff(0, 4);
    ledon(0, 3);
  }

  else if (rssid <= -60) {
    Serial.println("Getting Closer");
    display.println("Distance: ");
    display.println("Getting Closer");
    ledoff(0, 4);
    ledon(0, 2);
  }

  else {
    Serial.println("Away");
    display.print("Distance: ");
    display.println("Away");
    ledoff(0, 4);
    ledon(0, 1);
  }

  Serial.println("-------");
}

void scanAirTags() {
  while (true) {
    static unsigned long lastTime = millis();

    // Exit to main menu
    keyboard.loop();
    if (keyboard.left.isPressed()) {
      break;
    }

    // Scan for AirTags every 500ms
    if (millis() - lastTime > SCAN_DELAY_MS) {
      lastTime = millis();
      unsigned long start = millis();
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setCursor(0, 0);
      display.println(F("Scan Function"));
      display.println("");

      BLEDevice peripheral = BLE.available();

      if (peripheral) {
        int adLength = peripheral.advertisementData(advertisement, 31);

        if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00) {  // Check if it is an Apple AirTag
          display.clearDisplay();
          display.setTextSize(1);
          display.setTextColor(SH110X_WHITE);
          display.setCursor(0, 0);
          display.println(F("Airtag detected!"));
          display.println("");

          display.println("Address: ");
          display.println(peripheral.address());
          display.println("");
        } else {
          display.setTextSize(1);
          display.setTextColor(SH110X_WHITE);
          display.println("Found device, but ");
          display.println("it is not an AirTag");
          display.println("Address: ");
          display.println(peripheral.address());
        }
        display.display();
        Serial.println("Time: " + String(millis() - start) + "ms");
      }
    }
  }
}
