#pragma once

#define DIR_NAME       "warbee"
#define FORMAT_VERSION "ElecCats-1.0"
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

// Zigbee Packet fields
#define ZB_ADDRESS_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
// AP WiFi Packet fields
#define MAC_ADDRESS_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"
#define EMPTY_MAC_ADDRESS  "00:00:00:00:00:00"

// CSV file properties
#define CSV_LINE_SIZE    150  // Got it from real time tests
#define CSV_HEADER_LINES 2    // Check `csv_header` variable
#define MAX_CSV_LINES    200 + CSV_HEADER_LINES
#define CSV_FILE_SIZE    (CSV_LINE_SIZE) * (MAX_CSV_LINES)
