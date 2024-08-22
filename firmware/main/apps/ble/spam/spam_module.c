#include "apps/ble/spam/spam_module.h"
#include "animations_task.h"
#include "apps/ble/spam/spam_screens.h"
#include "bt_spam.h"
#include "esp_log.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"

static void ble_module_state_machine(uint8_t button_name, uint8_t button_event);

static void ble_module_state_machine(uint8_t button_name,
                                     uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN &&
      button_event != BUTTON_LONG_PRESS_HOLD) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      bt_spam_app_stop();
      menus_module_set_app_state(false, NULL);
      animations_task_stop();
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

void ble_module_begin() {
#if !defined(CONFIG_BLE_MODULE_DEBUG)
  esp_log_level_set(TAG_BLE_MODULE, ESP_LOG_NONE);
#endif
  menus_module_set_app_state(true, ble_module_state_machine);
  oled_screen_clear();
  ble_screens_start_scanning_animation();
  bt_spam_register_cb(ble_screens_display_scanning_text);
  bt_spam_app_main();
};