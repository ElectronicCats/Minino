#include <stdio.h>
#include "bluetooth_scanner.h"
#include "keyboard.h"
#include "leds.h"
#include "modules/menu_screens_modules.h"
#include "sd_card.h"

static const char* TAG = "main";

void app_main(void) {
  leds_init();
  leds_on();  // Indicate that the system is booting
  // return;
  sd_card_init();
  buzzer_init();
  wifi_sniffer_init();
  bluetooth_scanner_init();
  // thread_cli_init();
  menu_screens_init();
  // Init the keyboard after the display to avoid skipping the logo
  keyboard_init();
  leds_off();  // Indicate that the system is ready
}
