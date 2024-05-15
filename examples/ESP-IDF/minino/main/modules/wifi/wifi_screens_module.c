#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "modules/wifi/wifi_bitmaps.h"
#include "oled_screen.h"

int max_records_to_display = 7;

static const char* wifi_auth_modes[] = {"OPEN",
                                        "WEP",
                                        "WPA_PSK",
                                        "WPA2_PSK",
                                        "WPA_WPA2_PSK",
                                        "ENTERPRISE",
                                        "WPA3_PSK",
                                        "WPA2/3_PSK",
                                        "WAPI_PSK",
                                        "OWE",
                                        "WPA3_ENT_192",
                                        "WPA3_EXT_PSK",
                                        "WPA3EXTPSK_MIXED",
                                        "MAX"};

static const char* wifi_cipher_types[] = {
    "NONE",        "WEP40",       "WEP104", "TKIP", "CCMP",
    "TKIP_CCMP",   "AES_CMAC128", "SMS4",   "GCMP", "GCMP256",
    "AES_GMAC128", "AES_GMAC256", "UNKNOWN"};

void wifi_screens_module_scanning(void) {
  oled_screen_clear();
  oled_screen_display_text_center("SCANNING", 0, OLED_DISPLAY_NORMAL);
  while (true) {
    for (int i = 0; i < wifi_bitmap_allArray_LEN; i++) {
      oled_screen_display_bitmap(wifi_bitmap_allArray[i], 48, 16, 32, 32,
                                 OLED_DISPLAY_NORMAL);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void wifi_screens_module_animate_attacking(wifi_ap_record_t* ap_record) {
  oled_screen_clear();
  char* ssid = (char*) malloc(33);
  memset(ssid, 0, 33);
  sprintf(ssid, "%s", (char*) ap_record->ssid);

  oled_screen_display_text_center("TARGETING", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center(ssid, 1, OLED_DISPLAY_INVERT);

  while (true) {
    for (int i = 0; i < wifi_bitmap_allArray_LEN; i++) {
      oled_screen_display_bitmap(wifi_bitmap_allArray[i], 48, 16, 32, 32,
                                 OLED_DISPLAY_NORMAL);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  free(ssid);
}

void wifi_screens_module_display_scanned_networks(wifi_ap_record_t* ap_records,
                                                  int scanned_records,
                                                  int current_option) {
  oled_screen_clear();
  oled_screen_display_text_center("Select a network", 0, OLED_DISPLAY_NORMAL);

  for (int i = current_option; i < (max_records_to_display + current_option);
       i++) {
    if (i >= scanned_records) {
      break;
    }
    if (i == current_option) {
      char* prefix = "> ";
      char item_text[strlen(prefix) + strlen((char*) ap_records[i].ssid) + 1];
      strcpy(item_text, prefix);
      strcat(item_text, (char*) ap_records[i].ssid);
      oled_screen_display_text(item_text, 0, (i + 1) - current_option,
                               OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text((char*) ap_records[i].ssid, 0,
                               (i + 1) - current_option, OLED_DISPLAY_NORMAL);
    }
  }
}

void wifi_screens_module_display_details_network(wifi_ap_record_t* ap_record,
                                                 int page) {
  oled_screen_clear();
  char* ssid = (char*) malloc(33);
  memset(ssid, 0, 33);
  sprintf(ssid, "%s", (char*) ap_record->ssid);
  oled_screen_display_text_center(0, ssid, OLED_DISPLAY_INVERT);

  if (page == 0) {
    char* bssid = (char*) malloc(20);
    char* rssi_channel = (char*) malloc(MAX_LINE_CHAR);
    char* auth_mode = (char*) malloc(20);

    sprintf(auth_mode, "%s", wifi_auth_modes[ap_record->authmode]);
    sprintf(rssi_channel, "%d dBm   %d", ap_record->rssi, ap_record->primary);
    sprintf(bssid, "%02X:%02X:%02X:%02X:%02X%02X", ap_record->bssid[0],
            ap_record->bssid[1], ap_record->bssid[2], ap_record->bssid[3],
            ap_record->bssid[4], ap_record->bssid[5]);
    oled_screen_display_text_center("RSSI   PRIM CH", 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(rssi_channel, 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("BSSID", 4, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(bssid, 5, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("AUTH MODE", 6, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(auth_mode, 7, OLED_DISPLAY_NORMAL);
    free(bssid);
    free(rssi_channel);
    free(auth_mode);
  } else {
    char* pairwise_cipher = (char*) malloc(20);
    char* group_cipher = (char*) malloc(20);

    sprintf(pairwise_cipher, "%s",
            wifi_cipher_types[ap_record->pairwise_cipher]);
    sprintf(group_cipher, "%s", wifi_cipher_types[ap_record->group_cipher]);

    oled_screen_display_text_center("PAIRWISE CIPHER", 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(pairwise_cipher, 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center("GROUP CIPHER", 5, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(group_cipher, 6, OLED_DISPLAY_NORMAL);

    free(pairwise_cipher);
    free(group_cipher);
  }
  free(ssid);
}

void wifi_screens_module_display_attack_selector(char* attack_options[],
                                                 int list_count,
                                                 int current_option) {
  oled_screen_clear();
  oled_screen_display_text_center("Select Attack", 0, OLED_DISPLAY_NORMAL);
  for (int i = 0; i < list_count; i++) {
    if (attack_options[i] == NULL) {
      break;
    }

    if (i == current_option) {
      char* prefix = "> ";
      char item_text[strlen(prefix) + strlen(attack_options[i]) + 1];
      strcpy(item_text, prefix);
      strcat(item_text, attack_options[i]);
      oled_screen_display_text(item_text, 0, i + 1, OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text(attack_options[i], 0, i + 1,
                               OLED_DISPLAY_NORMAL);
    }
  }
}

void wifi_screens_module_display_captive_pass(char* ssid,
                                              char* user,
                                              char* pass) {
  oled_screen_clear();
  oled_screen_display_text_center("Captive Portal", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("SSID", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ssid, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("PASS", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(pass, 4, OLED_DISPLAY_INVERT);
}

void wifi_screens_module_display_captive_user_pass(char* ssid,
                                                   char* user,
                                                   char* pass) {
  oled_screen_clear();
  oled_screen_display_text_center("Captive Portal", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("SSID", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ssid, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("USER", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(user, 4, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("PASS", 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(pass, 6, OLED_DISPLAY_INVERT);
}

void wifi_screens_module_display_captive_selector(char* attack_options[],
                                                  int list_count,
                                                  int current_option) {
  oled_screen_clear();
  oled_screen_display_text_center("Select Portal", 0, OLED_DISPLAY_NORMAL);
  for (int i = 0; i < list_count; i++) {
    if (attack_options[i] == NULL) {
      break;
    }

    if (i == current_option) {
      char* prefix = "> ";
      char item_text[strlen(prefix) + strlen(attack_options[i]) + 1];
      strcpy(item_text, prefix);
      strcat(item_text, attack_options[i]);
      oled_screen_display_text(item_text, 0, i + 1, OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text(attack_options[i], 0, i + 1,
                               OLED_DISPLAY_NORMAL);
    }
  }
}
