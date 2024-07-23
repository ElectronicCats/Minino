#include <string.h>
#include "esp_log.h"

#include "sd_card.h"
#include "wardriving_module.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

#define FILE_NAME      "test.csv"
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

#define MAC_ADDRESS_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x"

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

char* get_mac_address(uint8_t* mac) {
  char* mac_address = malloc(18);
  sprintf(mac_address, MAC_ADDRESS_FORMAT, mac[0], mac[1], mac[2], mac[3],
          mac[4], mac[5]);
  return mac_address;
}

char* get_auth_mode(int authmode) {
  switch (authmode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2_WPA3_PSK";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI_PSK";
    case WIFI_AUTH_OWE:
      return "OWE";
    case WIFI_AUTH_WPA3_ENT_192:
      return "WPA3_ENT_SUITE_B_192_BIT";
    case WIFI_AUTH_DPP:
      return "DPP";
    default:
      return "Uncategorized";
  }
}

uint16_t get_frequency(uint8_t primary) {
  return 2412 + 5 * (primary - 1);
}

void scan_task(void* pvParameters) {
  wifi_driver_init_sta();
  while (true) {
    // Scan for WiFi networks
    wifi_scanner_module_scan();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void wardriving_begin() {
#if !defined(CONFIG_WARDRIVING_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  sd_card_mount();
  wifi_driver_init_sta();
  wifi_scanner_module_scan();

  uint16_t number = DEFAULT_SCAN_LIST_SIZE;
  // wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
  // uint16_t ap_count = 0;
  // memset(ap_info, 0, sizeof(ap_info));
  wifi_scanner_ap_records_t* ap_records = wifi_scanner_get_ap_records();

  char* csv_line = malloc(1024);
  char* csv_file = malloc(1024);

  // Append header to csv file
  sprintf(csv_file, "%s\n", csv_header);

  // Append records to csv file
  for (int i = 0; i < ap_records->count; i++) {
    sprintf(csv_line, "%s,%s,%s,%s,%d,%u,%d\n",
            get_mac_address(ap_records->records[i].bssid),
            ap_records->records[i].ssid,
            get_auth_mode(ap_records->records[i].authmode),
            "2021-09-01T00:00:00Z", ap_records->records[i].primary,
            get_frequency(ap_records->records[i].primary),
            ap_records->records[i].rssi);

    ESP_LOGI(TAG, "CSV Line: %s", csv_line);
    strcat(csv_file, csv_line);
  }
  sd_card_write_file(FILE_NAME, csv_file);
  free(csv_line);
  free(csv_file);

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

  sd_card_read_file(FILE_NAME);
  sd_card_unmount();
}
