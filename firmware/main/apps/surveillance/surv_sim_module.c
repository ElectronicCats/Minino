// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_sim_module.h"
#include <string.h>
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "radio_lease.h"
#include "surv_sim_screens.h"
#include "wifi_controller.h"
#include "app_manager.h"

#define TAG "surv_sim"

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx,
                            const void* buffer,
                            int len,
                            bool en_sys_seq);

static volatile bool s_sim_running = false;
static surv_sim_mode_t s_sim_mode = SURV_SIM_ALL;
static TaskHandle_t s_sim_task_handle = NULL;
static uint32_t s_packet_count = 0;
static uint16_t s_seq_num = 0;

/* WiFi and BLE cannot be up at the same time on the C6. SURV_SIM_ALL
   therefore alternates long windows instead of holding both stacks. */
#define SURV_SIM_WINDOW_MS 15000

typedef enum {
  SIM_RADIO_NONE = 0,
  SIM_RADIO_WIFI,
  SIM_RADIO_BLE,
} sim_radio_t;

static sim_radio_t s_radio = SIM_RADIO_NONE;

static void radio_leave(void) {
  if (s_radio == SIM_RADIO_WIFI) {
    radio_lease_wifi_release();
  } else if (s_radio == SIM_RADIO_BLE) {
    esp_ble_gap_stop_advertising();
    radio_lease_ble_release();
  }
  s_radio = SIM_RADIO_NONE;
}

static bool radio_enter_wifi(void) {
  if (s_radio == SIM_RADIO_WIFI) {
    return true;
  }
  radio_leave();
  esp_err_t err = radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "wifi lease: %s", esp_err_to_name(err));
    return false;
  }
  s_radio = SIM_RADIO_WIFI;
  wifi_driver_set_channel(11);
  return true;
}

static bool radio_enter_ble(void) {
  if (s_radio == SIM_RADIO_BLE) {
    return true;
  }
  radio_leave();
  esp_err_t err = radio_lease_ble_acquire();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ble lease: %s", esp_err_to_name(err));
    return false;
  }
  s_radio = SIM_RADIO_BLE;
  return true;
}

static esp_ble_adv_params_t s_ble_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x30,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_RANDOM,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// 802.11 Probe Request Frame matching Flock Safety Tier 4 (IE fingerprint)
static uint8_t s_flock_frame[] = {
    0x40, 0x00, 0x00, 0x00,  // Frame Control: Probe Request, Duration: 0
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // DA: Broadcast
    0x70, 0xc9, 0x4e, 0x12, 0x34, 0x56,  // SA: Flock Safety OUI (70:c9:4e)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // BSSID: Broadcast
    0x00, 0x00,                          // Sequence Control (offset 22)
    // Payload IEs matching surv_ie_matches_flock:
    0x00, 0x00,                             // Tag 0: len 0 (wildcard SSID)
    0x02, 0x02, 0xaa, 0xbb,                 // Tag 2
    0x0c, 0x01, 0x00,                       // Tag 12
    0x7f, 0x08, 0, 0, 0, 0, 0, 0, 0, 0x40,  // Tag 127
    0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,  // Vendor LiteON
    0x2d, 0x02, 0x00, 0x00,                                // Tag 45
    0xbf, 0x02, 0x00, 0x00,                                // Tag 191
    0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00   // Vendor WFA
};

// Apple AirTag BLE Advertisement payload (Total 31 bytes: 3 bytes Flags + 28
// bytes Apple MFR)
static const uint8_t s_airtag_payload[] = {
    0x02, 0x01, 0x1a,  // Flags: len 2, type 0x01 (Flags), val 0x1a
    0x1b, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x10,  // Apple MFR: len 27 (0x1b), type
                                               // 0xff, Apple ID 0x004c, sub
                                               // 0x12
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15};

// Axon BodyCam BLE Advertisement payload
static const uint8_t s_axon_payload[] = {
    0x02, 0x01, 0x06,  // Flags
    0x0c, 0x09, 'A',  'x', 'o', 'n', ' ',
    'B',  'o',  'd',  'y', ' ', '3'  // Complete Local Name
};

// Skimmer HC-05 BLE Advertisement payload
static const uint8_t s_skimmer_payload[] = {
    0x02, 0x01, 0x06,                     // Flags
    0x06, 0x09, 'H',  'C', '-', '0', '5'  // Complete Local Name HC-05
};

static void send_flock_wifi(void) {
  s_seq_num = (s_seq_num + 1) & 0x0FFF;
  s_flock_frame[22] = (uint8_t) ((s_seq_num << 4) & 0xF0);
  s_flock_frame[23] = (uint8_t) ((s_seq_num >> 4) & 0xFF);
  if (esp_wifi_80211_tx(WIFI_IF_STA, s_flock_frame, sizeof(s_flock_frame),
                        false) == ESP_OK) {
    s_packet_count++;
  }
}

static void send_ble_adv(const uint8_t* payload,
                         uint8_t len,
                         const uint8_t* oui,
                         uint32_t dwell_ms) {
  esp_bd_addr_t rand_mac;
  esp_fill_random(rand_mac, 6);
  if (oui != NULL) {
    memcpy(rand_mac, oui, 3);
  } else {
    rand_mac[0] |= 0xC0;  // Static random address bits per BLE spec
  }

  esp_ble_gap_set_rand_addr(rand_mac);
  esp_ble_gap_config_adv_data_raw((uint8_t*) payload, len);
  if (esp_ble_gap_start_advertising(&s_ble_adv_params) == ESP_OK) {
    s_packet_count++;
  }
  vTaskDelay(pdMS_TO_TICKS(dwell_ms));
  esp_ble_gap_stop_advertising();
  vTaskDelay(pdMS_TO_TICKS(30));
}

static void sim_worker_task(void* arg) {
  (void) arg;
  uint8_t cycle = 0;
  bool wifi_window = (s_sim_mode != SURV_SIM_AIRTAG &&
                      s_sim_mode != SURV_SIM_AXON &&
                      s_sim_mode != SURV_SIM_SKIMMER);
  TickType_t window_start = xTaskGetTickCount();

  while (s_sim_running) {
    const char* mode_name = "ALL";
    const char* tgt_name = "Flock T4";
    bool is_ble = false;

    if (s_sim_mode == SURV_SIM_ALL &&
        (xTaskGetTickCount() - window_start) * portTICK_PERIOD_MS >=
            SURV_SIM_WINDOW_MS) {
      wifi_window = !wifi_window;
      window_start = xTaskGetTickCount();
      radio_leave();
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    switch (s_sim_mode) {
      case SURV_SIM_FLOCK:
        mode_name = "FLOCK";
        tgt_name = "Flock Cam T4";
        if (!radio_enter_wifi()) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        send_flock_wifi();
        vTaskDelay(pdMS_TO_TICKS(200));
        break;

      case SURV_SIM_AIRTAG:
        mode_name = "AIRTAG";
        tgt_name = "Apple AirTag";
        is_ble = true;
        if (!radio_enter_ble()) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        send_ble_adv(s_airtag_payload, sizeof(s_airtag_payload), NULL, 600);
        break;

      case SURV_SIM_AXON: {
        mode_name = "AXON";
        tgt_name = "Axon BodyCam";
        is_ble = true;
        if (!radio_enter_ble()) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        const uint8_t axon_oui[3] = {0x00, 0x25, 0xdf};
        send_ble_adv(s_axon_payload, sizeof(s_axon_payload), axon_oui, 600);
        break;
      }

      case SURV_SIM_SKIMMER:
        mode_name = "SKIMMER";
        tgt_name = "Skimmer HC-05";
        is_ble = true;
        if (!radio_enter_ble()) {
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;
        }
        send_ble_adv(s_skimmer_payload, sizeof(s_skimmer_payload), NULL, 600);
        break;

      case SURV_SIM_ALL:
      default:
        mode_name = wifi_window ? "LOOP-WIFI" : "LOOP-BLE";
        if (wifi_window) {
          tgt_name = "Flock Cam T4";
          if (!radio_enter_wifi()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
          }
          send_flock_wifi();
          vTaskDelay(pdMS_TO_TICKS(300));
        } else {
          if (!radio_enter_ble()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
          }
          is_ble = true;
          if (cycle == 0) {
            tgt_name = "Apple AirTag";
            send_ble_adv(s_airtag_payload, sizeof(s_airtag_payload), NULL, 500);
          } else if (cycle == 1) {
            tgt_name = "Axon BodyCam";
            const uint8_t axon_oui[3] = {0x00, 0x25, 0xdf};
            send_ble_adv(s_axon_payload, sizeof(s_axon_payload), axon_oui, 500);
          } else {
            tgt_name = "Skimmer HC-05";
            send_ble_adv(s_skimmer_payload, sizeof(s_skimmer_payload), NULL, 500);
          }
          cycle = (cycle + 1) % 3;
        }
        break;
    }

    if (s_sim_running) {
      surv_sim_screens_show(mode_name, tgt_name, s_packet_count, 11, is_ble);
    }
  }

  radio_leave();
  s_sim_task_handle = NULL;
  vTaskDelete(NULL);
}

static void surv_sim_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name == BUTTON_LEFT) {
    surv_sim_stop();
  }
}

static void surv_sim_start(surv_sim_mode_t mode) {
  if (s_sim_running) {
    return;
  }
  s_sim_mode = mode;
  s_sim_running = true;
  s_packet_count = 0;
  s_radio = SIM_RADIO_NONE;

  menus_module_set_app_state(true, surv_sim_input_cb);

  if (xTaskCreate(sim_worker_task, "surv_sim", 4096, NULL, 5,
                  &s_sim_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create simulator task");
    s_sim_running = false;
    s_sim_task_handle = NULL;
  }
}

void surv_sim_begin_all(void) {
  surv_sim_start(SURV_SIM_ALL);
}

void surv_sim_begin_flock(void) {
  surv_sim_start(SURV_SIM_FLOCK);
}

void surv_sim_begin_airtag(void) {
  surv_sim_start(SURV_SIM_AIRTAG);
}

void surv_sim_begin_axon(void) {
  surv_sim_start(SURV_SIM_AXON);
}

void surv_sim_begin_skimmer(void) {
  surv_sim_start(SURV_SIM_SKIMMER);
}

void surv_sim_stop(void) {
  s_sim_running = false;
  for (int i = 0; s_sim_task_handle != NULL && i < 200; i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  radio_leave();
  oled_screen_clear();
  // app_manager_exit() will be called by on_exit
}


// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void surv_sim_on_exit(void);

static void surv_sim_on_enter(void) {
  surv_sim_start(SURV_SIM_FLOCK);
}

static void surv_sim_on_input(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name == BUTTON_LEFT) {
    surv_sim_on_exit();
  }
}

static void surv_sim_on_exit(void) {
  surv_sim_stop();
}

// Wrapper for menu entry
void app_manager_enter_wrapper_surv_sim_flock(void) {
  extern const minino_app_t surv_sim_app;
  app_manager_enter(&surv_sim_app);
}

// App definition for app_manager
const minino_app_t surv_sim_app = {
  .name = "surv_sim",
  .on_enter = surv_sim_on_enter,
  .on_input = surv_sim_on_input,
  .on_exit = surv_sim_on_exit,
};

