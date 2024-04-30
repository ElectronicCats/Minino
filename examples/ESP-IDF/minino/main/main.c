#include <stdio.h>
#include "bluetooth_scanner.h"
#include "esp_log.h"
#include "keyboard.h"
#include "leds.h"
#include "menu_screens_modules.h"
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
  leds_init();
  preferences_begin();
  sd_card_init();
  buzzer_init();
  wifi_sniffer_init();
  bluetooth_scanner_init();
  // thread_cli_init();
  menu_screens_init();
  // Init the keyboard after the display to avoid skipping the logo
  keyboard_init();
  reboot_counter();
  leds_off();  // Indicate that the system is ready
}
