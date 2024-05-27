#include <stdio.h>
#include "bluetooth_scanner.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "keyboard_module.h"
#include "leds.h"
#include "menu_screens_modules.h"
#include "openthread.h"
#include "preferences.h"
#include "sd_card.h"

static const char* TAG = "main";

void reboot_counter() {
  int32_t counter = preferences_get_int("reboot_counter", 0);
  ESP_LOGI(TAG, "Reboot counter: %ld", counter);
  counter++;
  preferences_put_int("reboot_counter", counter);
}

void app_main(void) {
  uint64_t start_time, end_time;
  start_time = esp_timer_get_time();

  leds_init();
  leds_on();
  preferences_begin();
  sd_card_init();
  // bluetooth_scanner_init();
  menu_screens_begin();
  keyboard_module_begin();
  menu_screens_display_menu();
  reboot_counter();
  leds_off();

  end_time = esp_timer_get_time();
  float time = (float) (end_time - start_time) / 1000000;
  printf("Total time taken: %2.2f seconds\n", time);
}
