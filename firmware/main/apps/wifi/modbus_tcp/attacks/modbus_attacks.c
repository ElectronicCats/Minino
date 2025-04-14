#include "modbus_attacks.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modbus_engine.h"

#define TAG "Modbus_attacks"

static volatile bool attacking = false;

static void writer_task() {
  attacking = true;
  if (modbus_engine_connect() < 0) {
    ESP_LOGE(TAG, "Connection Failed");
  } else {
    while (attacking) {
      modbus_engine_send_request();
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  modbus_engine_disconnect();
  vTaskDelete(NULL);
}

void modbus_attacks_writer() {
  xTaskCreate(writer_task, "writer_task", 4096, NULL, 10, NULL);
}

void modbus_attacks_stop_writer() {
  attacking = false;
}