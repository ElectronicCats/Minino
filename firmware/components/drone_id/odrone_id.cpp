#include "odrone_id.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <new>

#include "location.h"
#include "spoofer.h"

#define TAG "odrone_id"

static int num_spoofers = 16;
static Spoofer* spoofers = nullptr;   /* heap: one per configured drone */
static Spoofer* ble_drone = nullptr; /* heap: same reason */
static bool add_ble_drone = false;

static TaskHandle_t s_spoofing_task_handle = NULL;

static void spoofing_task(void* pvParameters) {
  while (1) {
    if (add_ble_drone && ble_drone != nullptr) {
      ble_drone->update();
    }
    for (int i = 0; i < num_spoofers; i++) {
      spoofers[i].update();
      vTaskDelay(pdMS_TO_TICKS(num_spoofers > 0 ? (200 / num_spoofers) : 200));
    }
    if (num_spoofers == 0 && !add_ble_drone) {
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

/* (Re)allocates the spoofer array for the requested number of drones. One
   spoofer is 2.3 KB, and 16 of them did not fit in the DRAM the SDR build
   variant has left, so they live in the heap and only for the drones the
   user asked for. Returns how many could be allocated. */
static int alloc_spoofers(int num_drones) {
  if (spoofers != nullptr) {
    delete[] spoofers;
    spoofers = nullptr;
  }
  if (num_drones <= 0) {
    return 0;
  }
  Spoofer* fresh = new (std::nothrow) Spoofer[num_drones];
  if (fresh == nullptr) {
    ESP_LOGW(TAG, "no heap for %d spoofers", num_drones);
    return 0;
  }
  spoofers = fresh;
  return num_drones;
}

extern "C" {
void odrone_id_set_num_spoofers(uint8_t num_drones) {
  if (num_drones > 16) {
    num_drones = 16;
  }
  int allocated = alloc_spoofers(num_drones);
  if (allocated != num_drones) {
    num_drones = (uint8_t) allocated;
  }
  num_spoofers = num_drones;
}

void odrone_id_set_wifi_ap(uint8_t channel) {
  set_wifi_ap((char*) "Mini Drone", channel);
}

void odrone_id_set_location(float latitude, float longitude) {
  if (add_ble_drone && ble_drone != nullptr) {
    ble_drone->updateLocation(latitude, longitude);
  }
  for (int i = 0; i < num_spoofers; i++) {
    spoofers[i].updateLocation(latitude, longitude);
  }
}

void odrone_id_begin(uint8_t num_drones,
                     uint8_t channel,
                     float latitude,
                     float longitude) {
  ESP_LOGI(TAG,
           "Launching app with coordinates: Latitude = %f, Longitude = %f\n",
           latitude, longitude);
  if (s_spoofing_task_handle != NULL) {
    odrone_id_stop();
  }
  if (num_drones > 16) {
    num_drones = 16;
  }
  num_spoofers = alloc_spoofers(num_drones);
  for (int i = 0; i < num_spoofers; i++) {
    spoofers[i].init();
    spoofers[i].updateLocation(latitude, longitude);
  }
  ble_drone = new (std::nothrow) Spoofer();
  if (ble_drone != nullptr) {
    ble_drone->init(true);
    ble_drone->updateLocation(latitude, longitude);
  }
  odrone_id_set_wifi_ap(channel);
  xTaskCreate(&spoofing_task, "spoofing_task", 4096, NULL, 5,
              &s_spoofing_task_handle);
}

void odrone_id_stop(void) {
  if (s_spoofing_task_handle != NULL) {
    vTaskDelete(s_spoofing_task_handle);
    s_spoofing_task_handle = NULL;
  }
  if (ble_drone != nullptr) {
    delete ble_drone;
    ble_drone = nullptr;
  }
  if (spoofers != nullptr) {
    delete[] spoofers;
    spoofers = nullptr;
    num_spoofers = 0;
  }
  /* Killing the task was not enough: transmit_ble2() had already started the
     softAP and esp_ble_gap_start_advertising(), so leaving the app left the
     device beaconing OpenDroneID from a hidden AP and from BLE, with no UI
     indication and no way to stop it. */
  wifi_mode_t mode = WIFI_MODE_NULL;
  if (esp_wifi_get_mode(&mode) == ESP_OK && mode != WIFI_MODE_NULL) {
    esp_wifi_stop();
  }
  /* Only touch GAP when Bluedroid is up; the API asserts ENABLED. */
  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    esp_ble_gap_stop_advertising();
  }
}

void odrone_id_set_ble_drone(bool enable) {
  add_ble_drone = enable;
}
}
