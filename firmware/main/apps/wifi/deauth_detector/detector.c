#include "detector.h"
#include <string.h>
#include "detector_scenes.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

#include "menus_module.h"
#include "oled_screen.h"

#define WIFI_CHANNEL_SWITCH_INTERVAL 1000

static const char* TAG = "DEAUTH_DETECTOR";
static uint8_t current_channel = 99;
static uint16_t total_deauth_packets_count = 0;
static uint16_t deauth_packets_count_list[14];
static volatile bool running = false;
static bool channel_hopping = false;

static TaskHandle_t channel_hopper_handle = NULL;

void deauth_detector_stop();

static void packet_handler(void* buf, wifi_promiscuous_pkt_type_t type) {
  static TickType_t last_update = 0;
  TickType_t current_time = xTaskGetTickCount();
  wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*) buf;
  wifi_pkt_rx_ctrl_t rx_ctrl = p->rx_ctrl;

  uint8_t* payload = p->payload;

  uint8_t frame_type = payload[0];

  if (frame_type == 0xA0 || frame_type == 0xC0) {
    uint8_t* mac = payload + 10;

    ESP_LOGI(TAG,
             "Packet received channel: %d from MAC address: "
             "%02x:%02x:%02x:%02x:%02x:%02x",
             rx_ctrl.channel, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Deauth packet detected");
    total_deauth_packets_count++;
    deauth_packets_count_list[rx_ctrl.channel - 1]++;
  }
  // 500 ms
  if ((current_time - last_update) * portTICK_PERIOD_MS >= 500) {
    if (channel_hopping) {
      detector_scenes_show_table(deauth_packets_count_list);
    } else {
      detector_scenes_show_count(total_deauth_packets_count, rx_ctrl.channel);
    }
    last_update = current_time;
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
  ESP_LOGI(TAG, "Input event: button=%d, event=%d", button_name, button_event);
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name != BUTTON_LEFT) {
    return;
  }
  ESP_LOGI(TAG, "BUTTON_LEFT pressed, stopping detector");
  deauth_detector_stop();
}

void deauth_detector_begin() {
  menus_module_set_app_state(true, deauth_detector_input_cb);
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
  esp_wifi_set_promiscuous_rx_cb(packet_handler);

  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting wifi: %s", esp_err_to_name(err));
    return;
  }
  uint8_t get_saved_channel =
      preferences_get_int("det_channel", current_channel);
  channel_hopping = get_saved_channel == 99;
  current_channel = channel_hopping ? 1 : get_saved_channel + 1;
  esp_wifi_set_channel(current_channel, WIFI_SECOND_CHAN_NONE);
  running = true;
  if (channel_hopping) {
    xTaskCreate(channel_hopper, "channel_hopper", 2048, NULL, 10,
                &channel_hopper_handle);
  }
}

void deauth_detector_stop() {
  running = false;
  if (channel_hopper_handle != NULL) {
    vTaskDelete(channel_hopper_handle);
    channel_hopper_handle = NULL;
  }
  esp_wifi_stop();
  esp_wifi_set_promiscuous(false);
  oled_screen_clear();
  detector_scenes_main_menu();
}