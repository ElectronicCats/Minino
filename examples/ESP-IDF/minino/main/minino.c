#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void hello_task(void *pvParameter) {
  while (1) {
    printf("Hello world from task!\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void app_main(void) {
  xTaskCreate(&hello_task, "hello_task", 2048, NULL, 5, NULL);
  printf("Hello world!\n");
}
