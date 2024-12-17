#include "detector.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "DEAUTH_DETECTOR";
static uint8_t current_channel = 5;

static void packet_handler(uint8_t* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*) buf;
  // Get the packet header
  wifi_pkt_rx_ctrl_t rx_ctrl = p->rx_ctrl;
  // Get the MAC address
  uint8_t* mac = p->payload;
  uint8_t pkt_type = buf[12];
  // Check if the packet is a deauth packet
  if (pkt_type == 0xA0 || pkt_type == 0xC0) {
    ESP_LOGI(TAG,
             "Packet received from MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Deauth packet detected");
  }
}

static void channel_hopper(void* pvParameters) {
  while (true) {
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    current_channel = (current_channel % 13) + 1;
  }
}

void deauth_detector_begin() {
  esp_err_t err = esp_netif_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  err = esp_wifi_init(&cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing wifi: %s", esp_err_to_name(err));
    return;
  }
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting storage: %s", esp_err_to_name(err));
    return;
  }
  err = esp_wifi_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting mode: %s", esp_err_to_name(err));
    return;
  }
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&packet_handler);
  esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting wifi: %s", esp_err_to_name(err));
    return;
  }

  // xTaskCreate(channel_hopper, "channel_hopper", 2048, NULL, 10, NULL);
}