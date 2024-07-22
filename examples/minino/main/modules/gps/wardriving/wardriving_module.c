#include "wardriving_module.h"
#include "sd_card.h"

#define FORMAT_VERSION "WigleWifi-1.6"
#define APP_VERSION    CONFIG_PROJECT_VERSION
#define MODEL          "MININO"
#define RELEASE        APP_VERSION
#define DEVICE         "MININO"
#define DISPLAY        "SH1106 OLED"
#define BOARD          "ESP32C6"
#define BRAND          "Electronic Cats"
#define STAR           "Sol"
#define BODY           "3"
#define SUB_BODY       "0"

const char* TAG = "wardriving";

const char* csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" APP_VERSION
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
    "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";

void scan_task(void* pvParameters) {
  while (true) {
    // Scan for WiFi networks
    // ...
  }
}

void wardriving_begin() {
  sd_card_mount();
  sd_card_write_file("test.csv", csv_header);
  sd_card_read_file("test.csv");
  sd_card_unmount();
}