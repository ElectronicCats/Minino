#include "odrone_id.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "location.h"
#include "spoofer.h"

#define TAG "odrone_id"

static int num_spoofers = 16;
static Spoofer spoofers[16];

static void spoofing_task(void* pvParameters) {
  while (1) {
    for (int i = 0; i < num_spoofers; i++) {
      spoofers[i].update();
      vTaskDelay(pdMS_TO_TICKS(200 / num_spoofers));
    }
  }
}

extern "C" {
void odrone_id_set_wifi_ap(uint8_t channel) {
  set_wifi_ap("Mini Drone", channel);
}

void odrone_id_begin(uint8_t num_drones,
                     uint8_t channel,
                     float latitude,
                     float longitude) {
  ESP_LOGI(TAG,
           "Launching app with coordinates: Latitude = %f, Longitude = %f\n",
           latitude, longitude);
  for (int i = 0; i < num_drones; i++) {
    spoofers[i].init();
    spoofers[i].updateLocation(latitude, longitude);
  }
  odrone_id_set_wifi_ap(channel);
  xTaskCreate(&spoofing_task, "spoofing_task", 4096, NULL, 5, NULL);
}
}
