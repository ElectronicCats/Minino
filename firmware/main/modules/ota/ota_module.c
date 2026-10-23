#include "ota_module.h"
#include "OTA.h"
#include "app_manager.h"
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
      app_manager_exit();
      break;
    case BUTTON_RIGHT:
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_BOOT:
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void ota_on_enter(void) {
  OTA_set_show_event_cb(ota_module_screens_show_event);
  ota_module_screens_show_help();
  OTA_init();
  menus_module_set_app_state(true, ota_module_input);
}

static void ota_on_input(uint8_t button_name, uint8_t button_event) {
  // Reuse existing input handler
  ota_module_input(button_name, button_event);
}

static void ota_on_exit(void) {
  // OTA doesn't need special cleanup; menus_module_restart() was the old exit
  // No resources to free here
}

// Wrapper for menu entry
void app_manager_enter_wrapper_ota(void) {
  extern const minino_app_t ota_app;
  app_manager_enter(&ota_app);
}

// App definition for app_manager
const minino_app_t ota_app = {
    .name = "ota",
    .on_enter = ota_on_enter,
    .on_input = ota_on_input,
    .on_exit = ota_on_exit,
};
