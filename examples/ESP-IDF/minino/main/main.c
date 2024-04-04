#include <stdio.h>
#include "bluetooth_scanner.h"
#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard.h"
#include "leds.h"
#include "sdcard.h"
#include "thread_cli.h"
#include "simple_sniffer.h"

static const char* TAG = "main";

void hello_task(void* pvParameter) {
  while (1) {
    // printf("Hello world from task!\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  simple_wifi_sniffer_init();
  return;
  leds_init();
  leds_on();
  sdcard_test();
  buzzer_init();
  bluetooth_scanner_init();
  thread_cli_init();
  display_init();
  keyboard_init();  // Init the keyboard after the display to avoid skipping
                    // the logo
  leds_off();
  xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
  printf("Hello world!\n");
}
