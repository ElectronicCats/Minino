#include <stdio.h>

#include "esp_system.h"
#include "sdkconfig.h"

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
#include "logs_output.h"
#include "menus_module.h"
#include "preferences.h"
#include "resistor_detector.h"
#include "sd_card.h"
#if defined(CONFIG_MININO_OPENTHREAD_STACK)
  #include "thread_sniffer.h"
#endif
#include "radio_lease.h"
#include "sleep_mode.h"
#include "surveillance_module.h"
#include "uart_bridge.h"
#include "wifi_service.h"

// Core components
#include "error_handler.h"
#include "esp_sdr_memory.h"
#include "memory_monitor.h"
#include "task_manager.h"

#if defined(CONFIG_MININO_ZIGBEE_STACK)
  #include "zb_cli.h"
#endif

#define BAUD_RATE        921600
#define UART_BUFFER_SIZE 1024
#define BUZZER_PIN       CONFIG_BUZZER_PIN

static const char* TAG = "main";

/* Flush and unmount the SD card on any deliberate esp_restart() (web file
   browser, Zigbee switch/light, SDR exit, console restart, the error
   handler). Without it those paths leave the FAT file system mid-write.
   Idempotent: sd_card_unmount() is a no-op when the card is not mounted.
   Not run on a panic, where nothing can safely touch the SD anyway. */
static void sd_card_unmount_on_restart(void) {
  sd_card_unmount();
}

void app_main() {
  // Before anything allocates: the SDR capture bank goes to the heap unless
  // the SDR app asked for this boot.
  bool sdr_boot = sdr_bank_boot();

  // Initialize core systems FIRST
  error_handler_init();
  task_manager_init();
  heap_monitor_init();
  heap_monitor_start(5000);  // Monitor every 5 seconds

  ESP_LOGI(TAG, "✅ Core components initialized");

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

  buzzer_begin(BUZZER_PIN);

  bool stealth_mode = preferences_get_bool("stealth_mode", false);
  if (!stealth_mode) {
    buzzer_enable();
    leds_begin();
  } else {
    buzzer_disable();
  }

  leds_off();
  sd_card_begin();
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_register_shutdown_handler(sd_card_unmount_on_restart));
  flash_fs_begin(flash_fs_screens_handler);
  keyboard_module_begin();
  menus_module_begin();
  wifi_service_set_init_guard(radio_lease_wifi_allowed);
  wifi_service_init(WIFI_MODE_STA);
#if defined(CONFIG_MININO_OPENTHREAD_STACK)
  thread_sniffer_subscribe_screen_events();
#endif
  sleep_mode_begin();
  preferences_put_bool("wifi_connected", false);
  flash_storage_begin();

  cat_console_begin();  // Spawns dedicated FreeRTOS console task

#if defined(CONFIG_SDR_ENABLE)
  if (sdr_boot) {
    menus_module_set_menu(MENU_ESP_SDR);
  }
#else
  (void) sdr_boot;
#endif
}
