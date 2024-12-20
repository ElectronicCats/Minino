#include "detector.h"
#include <string.h>
#include "detector_scenes.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

#include "menus_module.h"

#define WIFI_CHANNEL_SWITCH_INTERVAL 1000

static const char* TAG = "DEAUTH_DETECTOR";
static uint8_t current_channel = 99;
static uint16_t total_deauth_packets_count = 0;
static uint16_t deauth_packets_count_list[14];
static volatile bool running = false;
static bool channel_hopping = false;

void deauth_detector_stop();

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
             "Packet received channel: %d from MAC address: "
             "%02x:%02x:%02x:%02x:%02x:%02x",
             rx_ctrl.channel, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Deauth packet detected");
    total_deauth_packets_count++;
    deauth_packets_count_list[rx_ctrl.channel - 1]++;
  }
  if (channel_hopping) {
    detector_scenes_show_table(deauth_packets_count_list);
  } else {
    detector_scenes_show_count(total_deauth_packets_count, rx_ctrl.channel);
  }
}

static void channel_hopper(void* pvParameters) {
  esp_err_t err;
  while (running) {
    esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(WIFI_CHANNEL_SWITCH_INTERVAL / portTICK_PERIOD_MS);
    current_channel = (current_channel % 14) + 1;
    ESP_LOGI(TAG, "Switching to channel: %d", current_channel);
  }
  vTaskDelete(NULL);
}

static void deauth_detector_input_cb(uint8_t button_name,
                                     uint8_t button_event) {
  printf("FUNC: %s -> LINE: %d\n", __func__, __LINE__);
  if (button_event != BUTTON_PRESS_DOWN || button_name != BUTTON_LEFT) {
    return;
  }
  printf("FUNC: %s -> LINE: %d\n", __func__, __LINE__);
  deauth_detector_stop();
}

void deauth_detector_begin() {
  memset(deauth_packets_count_list, 0, sizeof(deauth_packets_count_list));
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

  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting wifi: %s", esp_err_to_name(err));
    return;
  }
  uint8_t get_saved_channel =
      preferences_get_int("det_channel", current_channel);
  if (get_saved_channel == 99) {
    channel_hopping = true;
    current_channel = 1;
  } else {
    current_channel = get_saved_channel + 1;
  }
  esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
  running = true;

  if (channel_hopping) {
    xTaskCreate(channel_hopper, "channel_hopper", 2048, NULL, 10, NULL);
  }
  menus_module_set_app_state(true, deauth_detector_input_cb);
}

void deauth_detector_stop() {
  esp_wifi_stop();
  esp_wifi_set_promiscuous(false);
  running = false;
  detector_scenes_main_menu();
}