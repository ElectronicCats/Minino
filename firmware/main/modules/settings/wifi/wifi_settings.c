#include <string.h>
#include "bitmaps_general.h"
#include "cmd_wifi.h"
#include "esp_log.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "menus_module.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wifi_ap_manager.h"
#include "wifi_settings_scenes.h"

#define TAG_CONFIG_MODULE "CONFIG_MODULE"

static void config_module_state_machine(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      wifi_ap_manager_unregister_callback();
      wifi_settings_scenes_main();
      break;
    case BUTTON_RIGHT:
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_BOOT:
    default:
      break;
  }
}

void wifi_settings_begin() {
  menus_module_set_app_state(true, config_module_state_machine);
}
