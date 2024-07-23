/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Console example — WiFi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "cmd_wifi.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "preferences.h"

#define JOIN_TIMEOUT_MS (5000)
#define MAXIMUM_RETRY   (3)

static const char* TAG = "cmd_wifi";

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static app_callback callback_connection;
static bool save_credentials = false;
static int connect(int argc, char** argv);

/** Arguments used by 'join' function */
static struct {
  struct arg_int* timeout;
  struct arg_str* ssid;
  struct arg_str* password;
  struct arg_end* end;
} join_args;

static struct {
  struct arg_str* index;
  struct arg_end* end;
} delete_args;

static void cmd_wifi_save_credentials(int argc, char** argv) {
  save_credentials = true;
  int nerrors = arg_parse(argc, argv, (void**) &join_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, join_args.end, argv[0]);
    ESP_LOGW(__func__, "Error parsing arguments");
    return;
  }
  ESP_LOGI(__func__, "Connecting to '%s'", join_args.ssid->sval[0]);
  connect_wifi(join_args.ssid->sval[0], join_args.password->sval[0], NULL);
}

static void cmd_wifi_delete_crendentials(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &delete_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, delete_args.end, argv[0]);
    ESP_LOGW(__func__, "Error parsing arguments");
    return;
  }
  int index = atoi(delete_args.index->sval[0]);
  int count = preferences_get_int("count_ap", 0);
  if (index >= count) {
    ESP_LOGW(__func__, "Index out of range");
    return;
  }
  char wifi_ap[100];
  char wifi_ssid[100];

  sprintf(wifi_ap, "wifi%d", index);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    return;
  }
  preferences_remove(wifi_ap);
  preferences_remove(wifi_ssid);
  ESP_LOGI(__func__, "Deleted AP %s", wifi_ssid);

  preferences_put_int("count_ap", count - 1);
}

static void cmd_wifi_handle_credentials(const char* ssid, const char* pass) {
  if (save_credentials) {
    ESP_LOGI(__func__, "Saving credentials");
    int get_count = preferences_get_int("count_ap", 0);
    preferences_put_int("count_ap", get_count + 1);
    char wifi_ap[100];
    sprintf(wifi_ap, "wifi%d", get_count);
    preferences_put_string(wifi_ap, ssid);
    preferences_put_string(ssid, pass);
  }
  save_credentials = false;
}

static int cmd_wifi_show_aps(int argc, char** argv) {
  int count = preferences_get_int("count_ap", 0);
  if (count == 0) {
    printf("No saved APs\n");
    return 0;
  }
  ESP_LOGI(__func__, "Saved APs: %d", count);
  for (int i = 0; i < count; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      ESP_LOGW(__func__, "Error getting AP");
      return 1;
    }
    printf("[%i] SSID: %s\n", i, wifi_ssid);
  }
  return 0;
}

static void event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    preferences_put_bool("wifi_connected", false);
    esp_wifi_connect();
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    preferences_put_bool("wifi_connected", true);
    if (callback_connection) {
      callback_connection();
    }
  }
}

static void initialise_wifi(void) {
  esp_log_level_set("wifi", ESP_LOG_WARN);
  static bool initialized = false;
  if (initialized) {
    return;
  }
  ESP_ERROR_CHECK(esp_netif_init());
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* ap_netif = esp_netif_create_default_wifi_ap();
  assert(ap_netif);
  esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_event_handler_register(
      WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &event_handler, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
  ESP_ERROR_CHECK(esp_wifi_start());
  initialized = true;
}

static bool wifi_join(const char* ssid, const char* pass, int timeout_ms) {
  initialise_wifi();
  wifi_config_t wifi_config = {0};
  strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  if (pass) {
    strlcpy((char*) wifi_config.sta.password, pass,
            sizeof(wifi_config.sta.password));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_wifi_connect();

  preferences_put_string("ssid", ssid);
  preferences_put_string("passwd", pass);

  int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE,
                                 pdTRUE, timeout_ms / portTICK_PERIOD_MS);
  return (bits & CONNECTED_BIT) != 0;
}

static int connect(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &join_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, join_args.end, argv[0]);
    return 1;
  }
  ESP_LOGI(__func__, "Connecting to '%s'", join_args.ssid->sval[0]);

  /* set default value*/
  if (join_args.timeout->count == 0) {
    join_args.timeout->ival[0] = JOIN_TIMEOUT_MS;
  }
  printf("timeout: %d\n", join_args.timeout->ival[0]);
  printf("ssid: %s\n", join_args.ssid->sval[0]);
  printf("password: %s\n", join_args.password->sval[0]);
  bool connected =
      wifi_join(join_args.ssid->sval[0], join_args.password->sval[0],
                join_args.timeout->ival[0]);
  if (!connected) {
    ESP_LOGW(__func__, "Connection timed out");
    return 1;
  }
  ESP_LOGI(__func__, "Connected");
  return 0;
}

int connect_wifi(const char* ssid, const char* pass, app_callback cb) {
  if (cb) {
    callback_connection = cb;
  }
  cmd_wifi_handle_credentials(ssid, pass);
  bool connected = wifi_join(ssid, pass, JOIN_TIMEOUT_MS);
  if (connected) {
    ESP_LOGI(__func__, "Connected");
    return 0;
  }
  ESP_LOGW(__func__, "Connection timed out");
  return 1;
}

bool is_wifi_connected() {
  return xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT;
}

void register_wifi(void) {
#if !defined(CONFIG_CMD_WIFI_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  join_args.timeout =
      arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
  join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
  join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
  join_args.end = arg_end(2);

  delete_args.index = arg_str1(NULL, NULL, "<index>", "Index of AP to delete");
  delete_args.end = arg_end(1);

  const esp_console_cmd_t join_cmd = {
      .command = "join",
      .help = "Join WiFi AP as a station, credentials are not saved",
      .hint = NULL,
      .func = &connect,
      .argtable = &join_args};

  esp_console_cmd_t save_cmd = {.command = "save",
                                .help = "Save WiFi AP credentials and join",
                                .hint = NULL,
                                .func = &cmd_wifi_save_credentials,
                                .argtable = &join_args};

  esp_console_cmd_t show_cmd = {.command = "list",
                                .help = "Show saved WiFi AP credentials",
                                .hint = NULL,
                                .func = &cmd_wifi_show_aps,
                                .argtable = NULL};

  esp_console_cmd_t delete_cmd = {.command = "delete",
                                  .help = "Delete saved WiFi AP credentials",
                                  .hint = NULL,
                                  .func = &cmd_wifi_delete_crendentials,
                                  .argtable = &delete_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&save_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&show_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&delete_cmd));
}
