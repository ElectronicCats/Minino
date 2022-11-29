#include <ArduinoBLE.h>

/*
  Salvador Mendoza - Metabase Q
  
  Scan and force to play a sound for Apple AirTags after 10 minutes of detection
  Aug 10th, 2022
*/


uint8_t limitDevices = 10;                        //How many devices able to detect (if this variable change, must change the next variables section)

unsigned long delayStart[10];                     //Activity per AirTag
bool actives[10];                                 //To check if the Airtag is around more than 10 minutes
String mairtags[10];                              //Save Mac AirTags for reference
uint8_t advertisement[31];                        //Save actual AirTag Advertising
uint8_t advertags[10][31];                        //Save active AirTags Advertinsings

uint8_t positiona = 0;                            //The last Active AirTag

bool playSoundm = false;                          //Write chrac to play a sound without waiting 10 minutes

float MINUTES = 10.2;                             //Minimum 10 minutes to make it play a sound
unsigned long ONEMINUTE = 60000;                  //1 minute
unsigned long DELAY_TIME = ONEMINUTE * MINUTES;

uint8_t delayRunning = 1;                         //True if still waiting for a delay to finish

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

void checkTimeout() {                             // Check if the 10 minutes has timed out per specific AirTag
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
    Serial.println("Connected");
    cn = true;
    
    Serial.println("Discovering attributes...");
    if (peripheral.discoverAttributes() ) {
      Serial.println("Attributes discovered");
    }
    else {
      Serial.println("Attribute discovery failed!");
      peripheral.disconnect();
      cn = false;
    }
  }
  else {
    Serial.println("Failed to connect!");
  }
  
  return cn;
}

void wcharc(BLEDevice peripheral){ //Play a sound
  BLECharacteristic nCharac = peripheral.characteristic("7dfc9001-7d1c-4951-86aa-8d9728f8d66c");
  if (nCharac) {
   Serial.print("Found characteristic! Writing..."); 
   Serial.println(nCharac.writeValue((byte)0x01));
  } else {
    Serial.println("Peripheral does NOT have that characteristic");
  }
}

void loop(){
  BLEDevice peripheral = BLE.available(); 
 // check if a peripheral has been discovered
  
  bool chkc = false, rtag = false;
  
  if (peripheral){
    int adLength = peripheral.advertisementData(advertisement,31);
    
    if (advertisement[0] == 0x1e && advertisement[2] == 0x4c && advertisement[3] == 0x00){ //Check if it is an Apple AirTag
      // print address
      Serial.println("-------"); 
      Serial.print("Detected AirTag!! - ");
      if (advertisement[4] == 0x12 && advertisement[6] == 0x10){
        Serial.print("Registered and active device");
        rtag = true;
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
        
      if(rtag == true) {
        checkTimeout();      
        for (uint8_t i = 0; i < limitDevices; i++){ //Save the AirTag in the array
          if (!mairtags[i].equals(peripheral.address())){
            if (mairtags[i].equals("")){
  
              chkc = conec(peripheral);
              if (chkc) {
                
                mairtags[i] = peripheral.address(); 
                Serial.print("Saved in memory "); Serial.println(mairtags[i]);Serial.print(" - Position: "); Serial.println(i);
              
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
            Serial.println("This AirTag is alredy in memory...");
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
      Serial.println("-------");
    }
    else {
      Serial.println("- Found device, but it is not an AirTag");
      Serial.print("\tAddress: "); Serial.print(peripheral.address()); Serial.println(peripheral.deviceName());
    }
  }
}
