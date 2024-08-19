#include <stdio.h>
#include "apps/ble/hid_device/hid_module.h"
#include "apps/ble/trackers/trackers_module.h"
#include "cat_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash_fs.h"
#include "flash_fs_screens.h"
#include "keyboard_module.h"
#include "leds.h"
#include "menu_screens_modules.h"
#include "menus_module.h"
#include "open_thread.h"
#include "preferences.h"
#include "sd_card.h"
#include "wardriving_module.h"
#include "web_file_browser.h"
#include "wifi_app.h"

#define BUZZER_PIN GPIO_NUM_2

static const char* TAG = "main";

void app_main() {
#if !defined(CONFIG_MAIN_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  preferences_begin();

  bool stealth_mode = preferences_get_bool("stealth_mode", false);
  if (!stealth_mode) {
    buzzer_enable();
    leds_begin();
  }
  buzzer_begin(BUZZER_PIN);
  sd_card_begin();
  flash_fs_begin(flash_fs_screens_handler);
  keyboard_module_begin();
  menus_module_begin();
  leds_off();
  preferences_put_bool("wifi_connected", false);
  // cat_console_begin();
}
