#include <string.h>
#include "esp_log.h"

#include "sd_card.h"
#include "wardriving_module.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

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
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
    "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";

static void print_auth_mode(int authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
      break;
    case WIFI_AUTH_OWE:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
      break;
    case WIFI_AUTH_WEP:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
      break;
    case WIFI_AUTH_WPA_PSK:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
      break;
    case WIFI_AUTH_WPA2_PSK:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
      break;
    case WIFI_AUTH_WPA_WPA2_PSK:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
      break;
    case WIFI_AUTH_ENTERPRISE:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
      break;
    case WIFI_AUTH_WPA3_PSK:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
      break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
      break;
    case WIFI_AUTH_WPA3_ENT_192:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
      break;
    default:
      ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
      break;
  }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher) {
  switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
      break;
    case WIFI_CIPHER_TYPE_AES_CMAC128:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
      break;
    case WIFI_CIPHER_TYPE_SMS4:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
      break;
    case WIFI_CIPHER_TYPE_GCMP:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
      break;
    case WIFI_CIPHER_TYPE_GCMP256:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
      break;
    default:
      ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
      break;
  }

  switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
      break;
    case WIFI_CIPHER_TYPE_WEP40:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
      break;
    case WIFI_CIPHER_TYPE_WEP104:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
      break;
    case WIFI_CIPHER_TYPE_TKIP:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
      break;
    case WIFI_CIPHER_TYPE_CCMP:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
      break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
      break;
    case WIFI_CIPHER_TYPE_SMS4:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
      break;
    case WIFI_CIPHER_TYPE_GCMP:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
      break;
    case WIFI_CIPHER_TYPE_GCMP256:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
      break;
    default:
      ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
      break;
  }
}

void scan_task(void* pvParameters) {
  wifi_driver_init_sta();
  wifi_scanner_module_scan();
  while (true) {
    // Scan for WiFi networks
    // ...
  }
}

void wardriving_begin() {
  sd_card_mount();
  sd_card_write_file("test.csv", csv_header);
  sd_card_read_file("test.csv");
  wifi_driver_init_sta();
  wifi_scanner_module_scan();

  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  // wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  // uint16_t ap_count = 0;
  // memset(ap_info, 0, sizeof(ap_info));
  wifi_scanner_ap_records_t* ap_records = wifi_scanner_get_ap_records();

  for (int i = 0; i < ap_records->count; i++) {
    ESP_LOGI(TAG, "SSID \t\t%s", ap_records->records[i].ssid);
    ESP_LOGI(TAG, "RSSI \t\t%d", ap_records->records[i].rssi);
    print_auth_mode(ap_records->records[i].authmode);
    if (ap_records->records[i].authmode != WIFI_AUTH_WEP) {
      print_cipher_type(ap_records->records[i].pairwise_cipher,
                        ap_records->records[i].group_cipher);
    }
    ESP_LOGI(TAG, "Channel \t\t%d", ap_records->records[i].primary);
  }

  sd_card_unmount();
}
