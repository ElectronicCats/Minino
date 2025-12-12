#include "z_switch_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "general/bitmaps_general.h"
#include "led_events.h"
#include "oled_screen.h"

static uint16_t hid_current_item = 0;

static void z_switch_display_creating_network() {
  oled_screen_clear();
  oled_screen_display_text_center("Creating network", 2, OLED_DISPLAY_NORMAL);
}

static void z_switch_display_waiting_device() {
  oled_screen_clear();
  oled_screen_display_text_center("Waiting for devices", 2,
                                  OLED_DISPLAY_NORMAL);
}

void z_switch_handle_connection_status(uint8_t status) {
  switch (status) {
    case CREATING_NETWORK:
      z_switch_display_creating_network();
      break;
    case CREATING_NETWORK_FAILED:
      break;
    case WAITING_FOR_DEVICES:
      z_switch_display_waiting_device();
      break;
    case NO_DEVICES_FOUND:
      break;
    case CLOSING_NETWORK:
      break;
    case LIGHT_PRESSED:
      break;
    case LIGHT_RELASED:
    default:
      break;
  }
}