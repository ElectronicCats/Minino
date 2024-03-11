#include <stdio.h>
#include "display.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard.h"

static const char* TAG = "main";

void hello_task(void* pvParameter) {
    while (1) {
        // printf("Hello world from task!\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    buzzer_init();
    display_init();
    keyboard_init();  // Init the keyboard after the display to avoid skipping the logo
    xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
    printf("Hello world!\n");
}
