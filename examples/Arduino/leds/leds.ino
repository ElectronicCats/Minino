#include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q
  
  LEDs detection - Apple AirTags 
  Aug 10th, 2022
*/

uint8_t leds[] = { 18, 19, 22, 21 }; //Frontier board - Bast
uint8_t advertisement[31]; //Save actual AirTag advertising packet

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  for (uint8_t i = 0; i < 4; i++){
    pinMode(leds[i], OUTPUT);
  }
  
  // Begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  Serial.println("BLE Central scan");

  // start scanning for peripheral
  BLE.scan();
}

void ledon(int from, int to){
  for (int i = from; i < to; i++){
    digitalWrite(leds[i], HIGH);
  }
}
void ledoff(int from, int to){
  for (int i = from; i < to; i++){
    digitalWrite(leds[i], LOW);
  }
}

void rssip(int rssid){                 //Check the signal power to know how far the AirTag is located
  Serial.println();
  
  Serial.print("RSSI: ");
  Serial.println(rssid);
  Serial.println();
  Serial.print("Distance: ");

  if (rssid > -30){
    Serial.println("Very close");
    for (int x = 0; x < 4; x++) {
      ledoff(0,4);
      delay(20);
      ledon(0,4);
    }
  }
  else if(rssid <= -30 && rssid > -60){
    Serial.println("Near");
    ledoff(0,4);
    ledon(0,3);
  }
  else if(rssid <= -60){
    Serial.println("Getting Closer");
    ledoff(0,4);
    ledon(0,2);
  }
  
  else {
    Serial.println("Away");
    ledoff(0,4);
    ledon(0,1);
  }
  
  Serial.println("-------"); 
}

void loop(){
  BLEDevice peripheral = BLE.available(); 
  
  if (peripheral){
    
    int adLength = peripheral.advertisementData(advertisement,31);
    
    if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00){ //Check if it is an Apple AirTag

      Serial.println("-------"); 
      Serial.print("Detected AirTag!! - ");
      
      if (advertisement[4] == 0x12 && advertisement[6] == 0x10){
        Serial.print("Registered and active device");
      }
      else if(advertisement[4] == 0x07 && advertisement[6] == 0x05){
        Serial.print("Unregister device");
      }
      
      Serial.print(" - Address: "); Serial.println(peripheral.address());
      Serial.print("Advertising data: ");
      for (int x = 0; x < 31; x++) {
        Serial.print(advertisement[x],HEX); Serial.print(" ");
      }
      Serial.println("");
      Serial.println("-------"); 
      
      rssip(peripheral.rssi());
    }
    else {
      Serial.println("- Found device, but it is not an AirTag");
      Serial.print("\tAddress: "); Serial.print(peripheral.address()); Serial.println(peripheral.deviceName());
    }
  }
}
