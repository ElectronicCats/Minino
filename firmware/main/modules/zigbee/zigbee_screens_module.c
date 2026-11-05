#include "zigbee_screens_module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_screens.h"
#include "oled_screen.h"
#include "sdkconfig.h"
#include "zigbee_bitmaps.h"
#include "zigbee_light.h"
#include "zigbee_switch.h"

void zigbee_screens_module_toogle_pressed(void) {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t width = 128;
  uint8_t height = 64;
#else
  uint8_t width = 128;
  uint8_t height = 32;
#endif

  oled_screen_display_bitmap(epd_bitmap_toggle_btn_pressed, 0, 0, width, height,
                             OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_toggle_released(void) {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t width = 128;
  uint8_t height = 64;
#else
  uint8_t width = 128;
  uint8_t height = 32;
#endif

  oled_screen_display_bitmap(epd_bitmap_toggle_btn_released, 0, 0, width,
                             height, OLED_DISPLAY_NORMAL);
}

void zigbee_screens_module_creating_network(void) {
  oled_screen_clear();
  genera_screen_display_card_information("Creating network", "Please wait...");
}

void zigbee_screens_module_creating_network_failed(void) {
  oled_screen_clear();
  genera_screen_display_card_information("Creating network", "Failed");
}

void zigbee_screens_module_waiting_for_devices(void) {
  static uint8_t dots = 0;
  dots = (dots + 1 > 3) ? 0 : (dots + 1);
  genera_screen_display_card_information("Waiting for", "devices");
  for (int i = 0; i < 3; i++) {
    oled_screen_display_text(i < dots ? "." : "", 56 + (i * 8), 4,
                             OLED_DISPLAY_NORMAL);
  }
}

void zigbee_screens_module_no_devices_found(void) {
  oled_screen_clear();
  vTaskDelay(pdMS_TO_TICKS(100));
  genera_screen_display_card_information("No devices", "found");
}

void zigbee_screens_module_closing_network(void) {
  oled_screen_clear();
  genera_screen_display_card_information("Closing network", "Please wait...");
}

void zigbee_screens_module_display_status(uint8_t status) {
  switch (status) {
    case CREATING_NETWORK:
      zigbee_screens_module_creating_network();
      break;
    case CREATING_NETWORK_FAILED:
      zigbee_screens_module_creating_network_failed();
      break;
    case WAITING_FOR_DEVICES:
      zigbee_screens_module_waiting_for_devices();
      break;
    case NO_DEVICES_FOUND:
      zigbee_screens_module_no_devices_found();
      break;
    case CLOSING_NETWORK:
      zigbee_screens_module_closing_network();
      break;
    case LIGHT_PRESSED:
      zigbee_screens_module_toogle_pressed();
      break;
    case LIGHT_RELASED:
      zigbee_screens_module_toggle_released();
      break;
    default:
      break;
  }
}

/* Zigbee Light screens */

void zigbee_screens_light_joining(void) {
  static uint8_t dots = 0;
  dots = (dots + 1 > 3) ? 0 : (dots + 1);
  genera_screen_display_card_information("ZB Light", "Joining...");
  for (int i = 0; i < 3; i++) {
    oled_screen_display_text(i < dots ? "." : "", 56 + (i * 8), 4,
                             OLED_DISPLAY_NORMAL);
  }
}

void zigbee_screens_light_joining_failed(void) {
  oled_screen_clear();
  genera_screen_display_card_information("ZB Light", "Join failed");
}

void zigbee_screens_light_on(void) {
  oled_screen_clear();
  genera_screen_display_card_information("ZB Light", "ON");
}

void zigbee_screens_light_off(void) {
  oled_screen_clear();
  genera_screen_display_card_information("ZB Light", "OFF");
}

void zigbee_screens_light_leaving(void) {
  oled_screen_clear();
  genera_screen_display_card_information("ZB Light", "Leaving...");
}

void zigbee_screens_light_disconnected(void) {
  oled_screen_clear();
  genera_screen_display_card_information("ZB Light", "Signal lost");
}

void zigbee_screens_light_display_status(uint8_t status) {
  switch (status) {
    case LIGHT_DISPLAY_JOINING:
      zigbee_screens_light_joining();
      break;
    case LIGHT_DISPLAY_JOINING_FAILED:
      zigbee_screens_light_joining_failed();
      break;
    case LIGHT_DISPLAY_ON:
      zigbee_screens_light_on();
      break;
    case LIGHT_DISPLAY_OFF:
      zigbee_screens_light_off();
      break;
    case LIGHT_DISPLAY_LEAVING:
      zigbee_screens_light_leaving();
      break;
    case LIGHT_DISPLAY_DISCONNECTED:
      zigbee_screens_light_disconnected();
      break;
    default:
      break;
  }
}

void zigbee_screens_display_device_ad(void) {
  oled_screen_clear();
  int index_page = 0;
  oled_screen_display_text_splited("Use our", &index_page, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("PyCatSniffer", &index_page,
                                   OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("to communicate", &index_page,
                                   OLED_DISPLAY_NORMAL);
  oled_screen_display_text_splited("with Wireshark", &index_page,
                                   OLED_DISPLAY_NORMAL);
}

void zigbee_screens_display_zigbee_sniffer_text(void) {
  oled_screen_clear();
#ifdef CONFIG_RESOLUTION_128X64
  oled_screen_display_text_center("ZIGBEE SNIFFER", 0, OLED_DISPLAY_NORMAL);
#else
  oled_screen_display_text("ZIGBEE", 52, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("SNIFFER", 48, 1, OLED_DISPLAY_NORMAL);
#endif
}

void zigbee_screens_display_scanning_animation(void) {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t x = 48;
  uint8_t y = 12;
#else
  uint8_t x = 0;
  uint8_t y = 0;
#endif
  static uint8_t frame = 0;
  oled_screen_display_bitmap(zigbee_bitmap_allArray[frame], x, y, 32, 32,
                             OLED_DISPLAY_NORMAL);
  frame = (frame + 1 >= zigbee_bitmap_allArray_LEN) ? 0 : (frame + 1);
}

void zigbee_screens_display_scanning_text(int count) {
  oled_screen_clear_buffer();
  char packets_count[24];
#ifdef CONFIG_RESOLUTION_128X64
  snprintf(packets_count, sizeof(packets_count), "Packets: %d", count);
  oled_screen_display_text_center(packets_count, 6, OLED_DISPLAY_INVERT);
#else
  snprintf(packets_count, sizeof(packets_count), "Packets:");
  oled_screen_display_text(packets_count, 40, 2, OLED_DISPLAY_INVERT);
  snprintf(packets_count, sizeof(packets_count), "%d", count);
  oled_screen_display_text(packets_count, 40, 3, OLED_DISPLAY_NORMAL);
#endif
  oled_screen_display_show();
}
