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

void reboot_counter() {
  int32_t counter = preferences_get_int("reboot_counter", 0);
  ESP_LOGI(TAG, "Reboot counter: %ld", counter);
  counter++;
  preferences_put_int("reboot_counter", counter);
}

void app_main() {
#if !defined(CONFIG_MAIN_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  uint64_t start_time, end_time;
  start_time = esp_timer_get_time();
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
  // menu_screens_begin();
  menus_module_begin();
  reboot_counter();
  leds_off();

  end_time = esp_timer_get_time();
  float time = (float) (end_time - start_time) / 1000000;
  char* time_str = malloc(sizeof(time) + 1);
  sprintf(time_str, "%2.2f", time);
  ESP_LOGI(TAG, "Total time taken: %s seconds", time_str);

  preferences_put_bool("wifi_connected", false);
  // cat_console_begin();
  // trackers_module_begin();
}
