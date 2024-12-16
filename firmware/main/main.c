#include <stdio.h>

#include "buzzer.h"
#include "cat_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash_fs.h"
#include "flash_fs_screens.h"
#include "keyboard_module.h"
#include "leds.h"
#include "menus_module.h"
#include "preferences.h"
#include "sd_card.h"

#include "driver/uart.h"

#define BAUD_RATE        115200
#define UART_BUFFER_SIZE 1024
#define BUZZER_PIN       GPIO_NUM_2

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

  uart_config_t uart_config = {
      .baud_rate = BAUD_RATE,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };
  uart_param_config(UART_NUM_0, &uart_config);
  uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_NUM_0, UART_BUFFER_SIZE * 2, 0, 0, NULL, 0);

  uint8_t* data = (uint8_t*) malloc(UART_BUFFER_SIZE);
  if (data == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for UART data");
    return;
  }

  ESP_LOGI(TAG, "Is UART0 initialized: %s",
           uart_is_driver_installed(UART_NUM_0) ? "true" : "false");
  ESP_LOGI(TAG, "Is UART1 initialized: %s",
           uart_is_driver_installed(UART_NUM_1) ? "true" : "false");

  while (true) {
    // int len = uart_read_bytes(UART_NUM_0, data, UART_BUFFER_SIZE, 20 /
    // portTICK_PERIOD_MS); if (len > 0) {
    //   ESP_LOGI(TAG, "Read %d bytes: %s", len, data);
    // }
    // printf("Bye, world!\n");
    ESP_LOGI(TAG, "Bye, world!");

    // Print "Hello world" to UART0
    const char* data = "Hello, world!\n";
    uart_write_bytes(UART_NUM_0, data, strlen(data));

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
