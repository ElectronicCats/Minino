/* Sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
// #if CONFIG_SNIFFER_PCAP_DESTINATION_SD
// #endif
#include "cmd_pcap.h"
#include "cmd_sniffer.h"
#include "sd_card.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

static const char* TAG = "WIFI_SNIFFER";

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

/* Initialize wifi with tcp/ip adapter */
void initialize_wifi(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

void wifi_sniffer_init() {
  /*--- Initialize Network ---*/
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  /* Initialize WiFi */
  initialize_wifi();

  register_sniffer_cmd();
  register_pcap_cmd();
}

// TODO: show summary on the display
void do_sniffer() {
  const char** summary_argv[] = {"pcap", "--summary", "-f", "sniffer"};
  uint8_t summary_argc = 4;
  do_pcap_cmd(summary_argc, (char**) summary_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void wifi_sniffer_start() {
  sd_card_mount();

  const char** pcap_argv[] = {"pcap", "--open", "-f", "sniffer"};
  uint8_t pcap_argc = 4;
  do_pcap_cmd(pcap_argc, (char**) pcap_argv); 

  const char** sniffer_argv[] = {"sniffer", "-i", "wlan", "-c",
                                 "2",       "-n", "2147483647"};
  uint8_t sniffer_argc = 7;
  do_sniffer_cmd(sniffer_argc, (char**) sniffer_argv);
}

void wifi_sniffer_stop() {
  const char** stop_argv[] = {"sniffer", "--stop"};
  uint8_t stop_argc = 2;
  do_sniffer_cmd(stop_argc, (char**) stop_argv);
  vTaskDelay(500 / portTICK_PERIOD_MS);

  const char** close_argv[] = {"pcap", "--close", "-f", "sniffer"};
  uint8_t close_argc = 4;
  do_pcap_cmd(close_argc, (char**) close_argv);
  vTaskDelay(100 / portTICK_PERIOD_MS);
}
