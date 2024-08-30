#include "apps/zigbee/z_switch/z_switch_module.h"
#include "apps/zigbee/z_switch/z_switch_screens.h"
#include "esp_log.h"
#include "menus_module.h"

static uint16_t current_item = 0;

static void z_switch_module_cb_event(uint8_t button_name, uint8_t button_event);
static void z_switch_module_cb_connection_handler(bool connection);

static void module_reset_menu() {
  current_item = 0;
  menus_module_set_app_state(true, z_switch_module_cb_event);
}

static void module_increment_item() {
  current_item++;
}

static void module_decrement_item() {
  current_item--;
}

static void z_switch_module_cb_connection_handler(bool connection) {}

static void z_switch_module_cb_event(uint8_t button_name,
                                     uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    default:
      break;
  }
}

void z_switch_module_begin() {
  radio_selector_set_zigbee_switch();
  zigbee_switch_set_display_status_cb(z_switch_handle_connection_status);
  zigbee_switch_init();
  menus_module_set_app_state(true, z_switch_module_cb_event);
}
