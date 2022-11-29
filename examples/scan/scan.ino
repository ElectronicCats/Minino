#include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q
  
  Scanning Apple AirTags 
  Aug 10th, 2022
*/

uint8_t advertisement[31]; //Save actual AirTag Advertising

void setup() {
  Serial.begin(9600);
  //while (!Serial);
  
  // Begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  Serial.println("BLE scan - filtering AirTags");

  // Start scanning for peripheral
  BLE.scan();
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
      
      Serial.print("RSSI: ");
      Serial.println(peripheral.rssi());
    }
    else {
      Serial.println("- Found device, but it is not an AirTag");
      Serial.print("\tAddress: "); Serial.print(peripheral.address()); Serial.println(peripheral.deviceName());
    }
  }
}
