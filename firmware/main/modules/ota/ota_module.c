#include "ota_module.h"
#include "OTA.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "ota_module_screens.h"

void ota_module_input(uint8_t button_name, uint8_t button_event);

void ota_module_init() {
  OTA_set_show_event_cb(ota_module_screens_show_event);
  ota_module_screens_show_help();
  OTA_init();
  menus_module_set_app_state(true, ota_module_input);
}

void ota_module_input(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_SINGLE_CLICK || is_ota_running) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_restart();
      break;
    case BUTTON_RIGHT:
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_BOOT:
    default:
      break;
  }
}