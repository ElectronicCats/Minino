/*
  Salvador Mendoza - Metabase Q

  Scan and force to play a sound for Apple AirTags after 10 minutes of detection
  Aug 10th, 2022
*/

void setupSound() {
  pinMode(BUZZ_PIN, OUTPUT);
  digitalWrite(BUZZ_PIN, LOW);
}

void playSound() {
  digitalWrite(BUZZ_PIN, HIGH);
}

void stopSound() {
  digitalWrite(BUZZ_PIN, LOW);
}

void checkTimeout() {  // Check if the 10 minutes has timed out per specific AirTag
  unsigned long nowTime = millis() - delayStart[positiona];
  if (nowTime >= DELAY_TIME) {
    // delayStart[positiona] += DELAY_TIME;         // This prevents drift in the delays
    actives[positiona] = true;
    // Serial.println("Timeout!!");
  } else {
    actives[positiona] = false;
  }
}

bool conec(BLEDevice peripheral) {  // Establish the BLE connection to the AirTag
  bool cn = false;
  BLE.stopScan();

  if (peripheral.connect()) {
    Serial.println("Connected");
    cn = true;

    Serial.println("Discovering attributes...");
    if (peripheral.discoverAttributes()) {
      Serial.println("Attributes discovered");
    } else {
      Serial.println("Attribute discovery failed!");
      peripheral.disconnect();
      cn = false;
    }
  } else {
    Serial.println("Failed to connect!");
  }

  return cn;
}

void wcharc(BLEDevice peripheral) {  // Play a sound
  BLECharacteristic nCharac = peripheral.characteristic("7dfc9001-7d1c-4951-86aa-8d9728f8d66c");
  if (nCharac) {
    Serial.print("Found characteristic! Writing...");
    Serial.println(nCharac.writeValue((byte)0x01));
  } else {
    Serial.println("Peripheral does NOT have that characteristic");
  }
}

void sound() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println(F("Test sound"));
  display.println("");
  display.println(F("Playing sound..."));
  display.display();

  while (true) {
    static unsigned long lastTime = millis();

    // Exit to main menu
    keyboard.loop();
    if (keyboard.left.isPressed()) {
      break;
    }

    playSound();
  }

  stopSound();
  return;

  BLEDevice peripheral = BLE.available();
  // check if a peripheral has been discovered

  bool chkc = false, rtag = false;

  if (peripheral) {
    int adLength = peripheral.advertisementData(advertisement, 31);

    if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00) {  // Check if it is an Apple AirTag
      // print address
      Serial.println("-------");
      Serial.print("Detected AirTag!! - ");
      if (advertisement[4] == 0x12 && advertisement[6] == 0x10) {
        Serial.print("Registered and active device");
        rtag = true;
      } else if (advertisement[4] == 0x07 && advertisement[6] == 0x05) {
        Serial.print("Unregister device");
      }

      Serial.print(" - Address: ");
      Serial.println(peripheral.address());
      Serial.print("Advertising data: ");
      for (int x = 0; x < 31; x++) {
        Serial.print(advertisement[x], HEX);
        Serial.print(" ");
      }
      Serial.println("");

      if (rtag == true) {
        checkTimeout();
        for (uint8_t i = 0; i < limitDevices; i++) {  // Save the AirTag in the array
          if (!mairtags[i].equals(peripheral.address())) {
            if (mairtags[i].equals("")) {
              chkc = conec(peripheral);
              if (chkc) {
                mairtags[i] = peripheral.address();
                Serial.print("Saved in memory ");
                Serial.println(mairtags[i]);
                Serial.print(" - Position: ");
                Serial.println(i);

                delayStart[i] = millis();
                positiona = i;
                delayRunning = 1;
                memcpy(&advertags[i][0], advertisement, sizeof(advertisement));
              }
              BLE.scan();
              delay(50);
              break;
            }
          } else {
            Serial.println("This AirTag is alredy in memory...");
            positiona = i;
            break;
          }
        }
      }

      if (actives[positiona] || playSoundm) {
        chkc = conec(peripheral);
        if (chkc) {
          wcharc(peripheral);
        }
        BLE.scan();
        delay(50);
      }
      Serial.println("-------");
    } else {
      Serial.println("- Found device, but it is not an AirTag");
      Serial.print("\tAddress: ");
      Serial.print(peripheral.address());
      Serial.println(peripheral.deviceName());
    }
  }
}
