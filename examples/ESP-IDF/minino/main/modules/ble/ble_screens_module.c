#include <string.h>
#include "modules/ble/ble_bitmaps.h"
#include "oled_screen.h"
#include "trackers_scanner.h"

void ble_screens_display_scanning_animation() {
  oled_screen_clear();
  oled_screen_display_text_center("BLE SPAM", 0, OLED_DISPLAY_NORMAL);
  while (true) {
    for (int i = 0; i < ble_bitmap_scan_attack_allArray_LEN; i++) {
      oled_screen_display_bitmap(ble_bitmap_scan_attack_allArray[i], 0, 16, 128,
                                 32, OLED_DISPLAY_NORMAL);
      vTaskDelay(500 / portTICK_PERIOD_MS);
    }
  }
}

void ble_screens_display_scanning_text(char* name) {
  oled_screen_clear_line(0, 7, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(name, 7, OLED_DISPLAY_INVERT);
}

void ble_screens_display_trackers_profiles(tracker_profile_t* trackers_scanned,
                                           int trackers_count,
                                           int device_selection) {
  char* name_str = (char*) malloc(50);
  oled_screen_display_text_center("Trackers Scanner", 0, OLED_DISPLAY_INVERT);
  int started_page = 2;
  for (int i_device = 0; i_device < trackers_count; i_device++) {
    oled_screen_clear_line(0, started_page, OLED_DISPLAY_NORMAL);
    sprintf(name_str, "%s RSSI: %d dBM", trackers_scanned[i_device].name,
            trackers_scanned[i_device].rssi);
    oled_screen_display_text_splited(name_str, &started_page,
                                     (device_selection == i_device)
                                         ? OLED_DISPLAY_INVERT
                                         : OLED_DISPLAY_NORMAL);
  }
  free(name_str);
}

void ble_screens_display_modal_trackers_profile(tracker_profile_t profile) {
  oled_screen_clear();
  int started_page = 1;
  char* name = (char*) malloc(MAX_LINE_CHAR);
  char* rssi = (char*) malloc(MAX_LINE_CHAR);
  char* mac_addrs = (char*) malloc(20);
  char* str_adv_data = (char*) malloc(64);

  memset(str_adv_data, 0, 64);
  sprintf(name, "%s", profile.name);
  sprintf(rssi, "RSSI: %d dBm", profile.rssi);
  sprintf(mac_addrs, "%02X:%02X:%02X:%02X:%02X%02X", profile.mac_address[0],
          profile.mac_address[1], profile.mac_address[2],
          profile.mac_address[3], profile.mac_address[4],
          profile.mac_address[5]);

  sprintf(str_adv_data, "ADV: ");
  for (int i = 96; i < 112; i++) {
    sprintf(str_adv_data + strlen(str_adv_data), "%02X ", profile.adv_data[i]);
  }
  oled_screen_display_text_center(name, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited(rssi, &started_page, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("MAC Address", started_page,
                                  OLED_DISPLAY_NORMAL);
  started_page++;
  oled_screen_display_text_center(mac_addrs, started_page, OLED_DISPLAY_NORMAL);
  started_page++;
  oled_screen_display_text_splited(str_adv_data, &started_page,
                                   OLED_DISPLAY_NORMAL);
  free(name);
  free(rssi);
  free(mac_addrs);
  free(str_adv_data);
}
