/* Sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your
   option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_pcap.h"
#include "cmd_sniffer.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "files_ops.h"
#include "flash_fs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "preferences.h"
#include "sd_card.h"
#include "sdkconfig.h"
#include "wifi_controller.h"
#include "wifi_sniffer.h"

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

static const char* TAG = "wifi_sniffer";

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

void wifi_sniffer_begin() {
#if !defined(CONFIG_WIFI_SNIFFER_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  wifi_driver_init_null();
  register_sniffer_cmd();
  register_pcap_cmd();
  ESP_LOGI(TAG, "Commands registered");
}

void wifi_sniffer_start() {
  ESP_LOGI(TAG, "Starting sniffer");

  if (wifi_sniffer_is_destination_sd()) {
    sd_card_mount();
  }

  const char** pcap_argv[] = {"pcap", "--open", "-f", "sniffer"};
  uint8_t pcap_argc = 4;
  do_pcap_cmd(pcap_argc, (char**) pcap_argv);

  char* channel_str = (char*) malloc(4);
  uint8_t channel = wifi_sniffer_get_channel();
  snprintf(channel_str, 4, "%d", channel);
  const char** sniffer_argv[] = {"sniffer",   "-i", "wlan",      "-c",
                                 channel_str, "-n", "2147483647"};
  uint8_t sniffer_argc = 7;
  do_sniffer_cmd(sniffer_argc, (char**) sniffer_argv);
}

void wifi_sniffer_stop() {
  const char** stop_argv[] = {"sniffer", "--stop"};
  uint8_t stop_argc = 2;
  do_sniffer_cmd(stop_argc, (char**) stop_argv);
}

void wifi_sniffer_close_file() {
  // wifi_sniffer_stop();

  const char** close_argv[] = {"pcap", "--close", "-f", "sniffer"};
  uint8_t close_argc = 4;
  do_pcap_cmd(close_argc, (char**) close_argv);
  if (wifi_sniffer_is_destination_sd()) {
    sd_card_unmount();
  }
}

void wifi_sniffer_load_summary() {
  const char** summary_argv[] = {"pcap", "--summary", "-f", "sniffer"};
  uint8_t summary_argc = 4;
  do_pcap_cmd(summary_argc, (char**) summary_argv);
}

uint8_t wifi_sniffer_get_channel() {
  return preferences_get_uint("wifi_channel", 1);
}

void wifi_sniffer_set_channel(uint8_t new_channel) {
  preferences_put_uint("wifi_channel", new_channel);
}

bool wifi_sniffer_is_destination_sd() {
  return preferences_get_bool("dest_sd", false);
}

bool wifi_sniffer_is_destination_internal() {
  return !wifi_sniffer_is_destination_sd();
}

void wifi_sniffer_set_destination_sd() {
  ESP_LOGI(TAG, "Setting destination to SD");
  preferences_put_bool("dest_sd", true);
}

void wifi_sniffer_set_destination_internal() {
  ESP_LOGI(TAG, "Setting destination to internal");
  preferences_put_bool("dest_sd", false);
}
