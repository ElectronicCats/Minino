#include "enum.h"

#include <Wire.h>
#include <SPI.h>
#include <ArduinoBLE.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>


Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

BLEService myService("fff0");
BLEIntCharacteristic myCharacteristic("fff1", BLERead | BLEBroadcast);
const uint8_t manufactData[4] = {0x01, 0x02, 0x03, 0x04};
const uint8_t serviceData[3] = {0x00, 0x01, 0x02};


void setup() {
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);
  pinMode(10,INPUT_PULLUP);
 myService.addCharacteristic(myCharacteristic);

  BLE.addService(myService);

  // Build scan response data packet
  BLEAdvertisingData scanData;

  // Set parameters for scan response packet
  //scanData.setLocalName("Test enhanced advertising");
  // Copy set parameters in the actual scan response packet
  //BLE.setScanResponseData(scanData);
  // Build advertising data packet

  
  Serial.begin(9600);

  // Begin initialization

  if (!display.begin(SCREEN_ADDRESS)) {
    //Serial.println(F("SH110X allocation failed"));
    for (;;); // Don't proceed, loop forever
  }
  display.clearDisplay(); 
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("BIENVENIDO");
  display.display();


  display.drawLine(0, 10, 128, 10, SH110X_WHITE);
  display.display();
  display.drawBitmap( (display.width() - LOGO_WIDTH ) / 2,((display.height()- LOGO_HEIGHT) / 2 )+4, logo, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
  display.display();
  delay(2000); 
  if (!BLE.begin()) {
   while (1);
  }
    BLE.scan();
    
}
  
void loop() {
  displayMenu();

}

void displayMenu() {

  int down = digitalRead(5);
  int up = digitalRead(6);
  int enter = digitalRead(9);
  int back = digitalRead(10);

  if (up == LOW && down == LOW) {
  };
  if (up == LOW) {
    selected++;
    delay(200);
  }
   if(selected==4){
        selected=0;}
  if (down == LOW) {
    selected--;
    delay(200);
  }
  if(selected==-1){
        selected=3;
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
    //display.print(selected);
    display.println("");
    for (int i = 0; i < 4; i++) {
      if (i == selected) {
        display.setTextColor(SH110X_BLACK, SH110X_WHITE);
        display.println(options[i]);
      } 
      else if (i != selected) {
        display.setTextColor(SH110X_WHITE);
        display.println(options[i]);
      }
    }
  } 
  if (Layer == 0) {
    Scanner();
    //llamar funcion de scan 
    //una vez presionado back sale de funcion
  }
  if (Layer ==1) {
    spoofDevice();
       //llamar funcion de spoof 
      //una vez presionado back sale de funcion
  }
   if (Layer == 2) {
 detect();
  }
   if (Layer == 3) {
   soundDevice();
     //llamar funcion de sound 
    //una vez presionado back sale de funcion
  }
   
   

 display.display();
}

//--------------------------------Init scan device Function------------------------------------//
//una vez presionado back sale de funcion
void Scanner(){
    
    //display.println("BLE scan - filtering AirTags");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Scan Function"));
    delay(1000);

  // Start scanning for peripheral
BLEDevice peripheral = BLE.available(); 

  if (peripheral){
    
      
    
    int adLength = peripheral.advertisementData(advertisement,31);
    if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00){ //Check if it is an Apple AirTag
        
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Airtags"));
    //display.print(selected);
    Serial.print("Address: "); Serial.println(peripheral.address());

    display.print("Address: "); display.println(peripheral.address());

    delay(2000);
    
    
     /* if (advertisement[4] == 0x12 && advertisement[6] == 0x10){
  
       display.clearDisplay(); 
       display.setTextSize(1);
       display.setTextColor(SH110X_WHITE);
       display.println("Registered and active device");
       delay(2000);
      }
      else if(advertisement[4] == 0x07 && advertisement[6] == 0x05){
       // Serial.print("Unregister device");
      }
  
      
      display.print("Address: "); display.println(peripheral.address());
      display.print("Advertising data: ");
      for (int x = 0; x < 31; x++) {
        display.print(advertisement[x],HEX); display.print(" ");
      }
      //Serial.println("");
      //Serial.println("-------"); 
      
      display.print("RSSI: ");
      display.println(peripheral.rssi());
 */ }
    else {
      
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.println("Found device, but ");
      display.println("it is not an AirTag");
      
      
  
  }
 }
}

//--------------------------------Init spoof device Function------------------------------------//



void spoofDevice(){
  

   display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Spoof Function");

  BLEAdvertisingData advData;

  uint8_t adv[27];
  for (int x = 4; x < 31; x++) {
    adv[x-4] = (byte)advertisement[x];
  }

  uint16_t idm = 0x004c; //Apple ID
  
  // Set parameters for advertising packet
  advData.setManufacturerData(idm, adv, sizeof(adv));
  advData.setAdvertisedService(myService);
  advData.setAdvertisedServiceData(0xfff0, serviceData, sizeof(serviceData));

  // Copy set parameters in the actual advertising packet
  BLE.setAdvertisingData(advData);

  BLE.advertise();
  display.println("spoofing ...");

  
 
}

void Poll(){
      BLE.poll();

}
//--------------------------------Init sound device Function------------------------------------//


void checkTimeout() {
    unsigned long nowTime = millis() - delayStart[positiona];
  if (nowTime >= DELAY_TIME) {
    //delayStart[positiona] += DELAY_TIME;         // This prevents drift in the delays
    actives[positiona] = true;
    //Serial.println("Timeout!!");
  }
  else{
    actives[positiona] = false;
  }
}
bool conec (BLEDevice peripheral){                //Establish the BLE connection to the AirTag
  bool cn = false;
  BLE.stopScan();
  
  if (peripheral.connect()) {
    display.println("Connected");
    cn = true;
    
    display.println("Discovering attributes...");
    if (peripheral.discoverAttributes() ) {
      display.println("Attributes discovered");
    }
    else {
      display.println("Attribute discovery failed!");
      peripheral.disconnect();
      cn = false;
    }
  }
  else {
    display.println("Failed to connect!");
  }
  
  return cn;
}
void wcharc(BLEDevice peripheral){ //Play a sound
  BLECharacteristic nCharac = peripheral.characteristic("7dfc9001-7d1c-4951-86aa-8d9728f8d66c");
  if (nCharac) {
   display.print("Found characteristic! Writing..."); 
   display.println(nCharac.writeValue((byte)0x01));
  } else {
    display.println("Peripheral does NOT have that characteristic");
  }
}
void soundDevice(){

     display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Sound Function");
    delay(1000);
  BLEDevice peripheral = BLE.available(); 
 // check if a peripheral has been discovered
  
  bool chkc = false, rtag = false;
  
  if (peripheral){
    int adLength = peripheral.advertisementData(advertisement,31);
    
    if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00){ //Check if it is an Apple AirTag
      // print address
      display.println("-------"); 
      display.print("Detected AirTag!! - ");
      if (advertisement[4] == 0x12 && advertisement[6] == 0x10){
        display.print("Registered and active device");
        rtag = true;
      }
      else if(advertisement[4] == 0x07 && advertisement[6] == 0x05){
        display.print("Unregister device");
      }
      
      display.print(" - Address: "); display.println(peripheral.address());
      display.print("Advertising data: ");
      for (int x = 0; x < 31; x++) {
        display.print(advertisement[x],HEX); display.print(" ");
      }
      display.println("");
        
      if(rtag == true) {
        checkTimeout();      
        for (uint8_t i = 0; i < limitDevices; i++){ //Save the AirTag in the array
          if (!mairtags[i].equals(peripheral.address())){
            if (mairtags[i].equals("")){
  
              chkc = conec(peripheral);
              if (chkc) {
                
                mairtags[i] = peripheral.address(); 
                display.print("Saved in memory "); display.println(mairtags[i]);display.print(" - Position: "); display.println(i);
              
                delayStart[i] = millis();
                positiona = i;
                delayRunning = 1;
                memcpy(&advertags[i][0], advertisement, sizeof(advertisement));
              }
              BLE.scan();
              delay(50);
              break;
            }
          }
          else {
            display.println("This AirTag is alredy in memory...");
            positiona = i;
            break;
          }
        }
      }          
      
      if (actives[positiona] || playSoundm){
        chkc = conec(peripheral);
        if (chkc){
          wcharc(peripheral);
        }
        BLE.scan();
        delay(50);
      }
      display.println("-------");
    }
    else {
      display.println("Found device, but");
      display.println("it is not an AirTag");
      display.println("Address:"); display.println(peripheral.address()); display.println(peripheral.deviceName());
      delay(1000);
    }
  }
}
  //--------------------------------Init detect device Function------------------------------------//
void detect(){
  display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(F("Detect Function"));
    display.println("configuration");
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.println("Detecting...");
}
