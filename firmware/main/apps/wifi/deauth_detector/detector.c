#include "wifi_controller.h"
#include "detector.h"
#include "radio_lease.h"
#include <string.h>
#include "detector_scenes.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"

#include "menus_module.h"
#include "app_manager.h"
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
    if (rx_ctrl.channel >= 1 && rx_ctrl.channel <= 14) {
      deauth_packets_count_list[rx_ctrl.channel - 1]++;
    }
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
  while (running) {
    wifi_driver_set_channel(current_channel);
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
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_netif_init: %s", esp_err_to_name(err));
    return;
  }
  if (!wifi_service_is_up_get() && !wifi_service_init(WIFI_MODE_STA)) {
    ESP_LOGE(TAG, "WiFi stack init failed");
    return;
  }
  err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting storage: %s", esp_err_to_name(err));
    return;
  }
  err = wifi_service_set_mode(WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error setting mode: %s", esp_err_to_name(err));
    return;
  }
  /* Claim the shared promiscuous slot instead of taking it: the callback and
     the on/off switch are global to the driver, and the surveillance detector
     used to overwrite this handler and switch promiscuous mode off at the end of
     its window, which froze the deauth counter. */
  if (radio_lease_prom_acquire(RADIO_PROM_DEAUTH_DETECTOR, packet_handler) !=
      ESP_OK) {
    ESP_LOGE(TAG, "could not claim promiscuous mode");
    return;
  }

  err = esp_wifi_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting wifi: %s", esp_err_to_name(err));
    return;
  }
  uint8_t get_saved_channel =
      preferences_get_int("det_channel", current_channel);
  channel_hopping = get_saved_channel == 99;
  current_channel = channel_hopping ? 1 : get_saved_channel + 1;
  wifi_driver_set_channel(current_channel);
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
  /* Order matters: esp_wifi_stop() leaves the driver initialised but stopped,
     and the analyzer's sniffer then calls esp_wifi_set_promiscuous() /
     esp_wifi_set_channel() against a stopped driver - both fail silently and
     the packet counter stays at zero. Clear promiscuous mode while the driver is
     still running, then leave Wi-Fi running but idle. */
  radio_lease_prom_release(RADIO_PROM_DEAUTH_DETECTOR);
  esp_wifi_stop();
  oled_screen_clear();
  detector_scenes_main_menu();
}


// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void deauth_detector_on_enter(void) {
  detector_scenes_main_menu();
}

static void deauth_detector_on_input(uint8_t button_name, uint8_t button_event) {
  // Scenes install their own input handlers.
  (void)button_name;
  (void)button_event;
}

static void deauth_detector_on_exit(void) {
  // Stop detector if running
  if (running) {
    deauth_detector_stop();
  }
  // Ensure promiscuous lease is released
  radio_lease_prom_release(RADIO_PROM_DEAUTH_DETECTOR);
  // Reset state
  running = false;
  channel_hopping = false;
  current_channel = 99;
  total_deauth_packets_count = 0;
  memset(deauth_packets_count_list, 0, sizeof(deauth_packets_count_list));
  if (channel_hopper_handle != NULL) {
    vTaskDelete(channel_hopper_handle);
    channel_hopper_handle = NULL;
  }
}

// Wrapper for menu entry
void app_manager_enter_wrapper_deauth_detector(void) {
  extern const minino_app_t deauth_detector_app;
  app_manager_enter(&deauth_detector_app);
}

// App definition for app_manager
const minino_app_t deauth_detector_app = {
  .name = "deauth_detector",
  .on_enter = deauth_detector_on_enter,
  .on_input = deauth_detector_on_input,
  .on_exit = deauth_detector_on_exit,
};

