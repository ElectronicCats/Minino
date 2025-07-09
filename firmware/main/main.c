#include <stdio.h>

#include "buzzer.h"
#include "cat_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash_fs.h"
#include "flash_fs_screens.h"
#include "general_flash_storage.h"
#include "gps_hw.h"
#include "keyboard_module.h"
#include "leds.h"
#include "menus_module.h"
#include "preferences.h"
#include "resistor_detector.h"
#include "sd_card.h"
#include "sleep_mode.h"
#include "uart_bridge.h"

#include "zb_cli.h"

#define BAUD_RATE        921600
#define UART_BUFFER_SIZE 1024
#define BUZZER_PIN       CONFIG_BUZZER_PIN

static const char* TAG = "main";
void app_main() {
  preferences_begin();
  gps_hw_init();
  sleep_mode_set_mode(resistor_detector(CONFIG_GPIO_RIGHT_BUTTON));
#if !defined(CONFIG_MAIN_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  uart_config_t uart_config = {
      .baud_rate = BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  uart_bridge_begin(uart_config, UART_BUFFER_SIZE);
  logs_output_set_output(preferences_get_uchar("logs_output", USB));

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
  flash_storage_begin();

  // gatt_read_file();

  cat_console_begin();  // Contains a while(true) loop, it must be at the end

  // zb_cli_begin();
}
