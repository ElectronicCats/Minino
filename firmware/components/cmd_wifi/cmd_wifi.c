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
static bool disconnect_cb = false;
static int reconnections = 0;
static int connect(int argc, char** argv);
static bool wifi_join(const char* ssid, const char* pass, int timeout_ms);

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

static struct {
  struct arg_str* index;
  struct arg_end* end;
} connect_args;

static int cmd_wifi_connect_index(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &connect_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, connect_args.end, argv[0]);
    ESP_LOGW(__func__, "Error parsing arguments");
    return 1;
  }
  int count = preferences_get_int("count_ap", 0);
  int index = atoi(connect_args.index->sval[0]);
  if (index > count) {
    ESP_LOGW(__func__, "Index out of range");
    return 1;
  }
  char wifi_ap[100];
  char wifi_ssid[100];
  sprintf(wifi_ap, "wifi%d", index);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    return 1;
  }
  char wifi_pass[100];
  err = preferences_get_string(wifi_ssid, wifi_pass, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    return 1;
  }

  join_args.ssid->sval[0] = wifi_ssid;
  join_args.password->sval[0] = wifi_pass;
  join_args.timeout->ival[0] = JOIN_TIMEOUT_MS;
  const char** sniffer_argv[] = {"join", join_args.ssid->sval[0],
                                 join_args.password->sval[0]};
  uint8_t sniffer_argc = 3;
  connect(sniffer_argc, (char**) sniffer_argv);
  connect(sniffer_argc, (char**) sniffer_argv);
  return 0;
}

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

  // Restore the AP indexes
  int counter = 0;
  for (int i = 0; i < count - 1; i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      continue;
    }
    char wifi_pass[100];
    err = preferences_get_string(wifi_ssid, wifi_pass, 100);
    if (err != ESP_OK) {
      continue;
    }
    char wifi_ap_new[100];
    sprintf(wifi_ap_new, "wifi%d", counter);
    preferences_put_string(wifi_ap_new, wifi_ssid);
    counter++;
  }

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
      continue;
    }
    printf("[%i][%s] SSID: %s\n", i, wifi_ap, wifi_ssid);
  }
  return 0;
}

static void event_handler(void* arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void* event_data) {
  printf("event_handler %ld\n", event_id);
  if (callback_connection &&
      (event_id == IP_EVENT_STA_GOT_IP || event_id == WIFI_EVENT_WIFI_READY)) {
    preferences_put_bool("wifi_connected", true);
    callback_connection(true);
  }
  if (callback_connection && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (reconnections == 0) {
      callback_connection(false);
    }
  }

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    preferences_put_bool("wifi_connected", false);
    preferences_remove("ssid");
    preferences_remove("passwd");
    if (reconnections < 3) {
      reconnections++;
      esp_wifi_connect();
    }
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    printf("Connected to AP");
    preferences_put_bool("wifi_connected", true);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
  }
}

static void initialise_wifi(void) {
  esp_log_level_set(TAG, ESP_LOG_WARN);
  static bool initialized = false;
  if (initialized) {
    return;
  }
  ESP_ERROR_CHECK(esp_netif_init());
  wifi_event_group = xEventGroupCreate();
  esp_err_t err = esp_event_loop_create_default();
  if (err == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Event loop already created");
  } else if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error creating event loop: %s", esp_err_to_name(err));
    esp_restart();
  }
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

  // Get the IP address of the ESP32 station
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(sta_netif, &ip_info);
  char ip_address[16];
  sprintf(ip_address, IPSTR, IP2STR(&ip_info.ip));

  ESP_LOGI(TAG, "IP Address: %s", ip_address);
}

static bool wifi_join(const char* ssid, const char* pass, int timeout_ms) {
  initialise_wifi();
  preferences_put_bool("wifi_connected", false);
  preferences_remove("ssid");
  preferences_remove("passwd");
  wifi_config_t wifi_config = {0};
  strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  if (pass) {
    strlcpy((char*) wifi_config.sta.password, pass,
            sizeof(wifi_config.sta.password));
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_err_t err = esp_wifi_connect();
  if (err != ESP_OK) {
    ESP_LOGE(__func__, "Failed to connect to AP");
    return false;
  }

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

void cmd_wifi_unregister_callback() {
  callback_connection = NULL;
}

int connect_wifi(const char* ssid, const char* pass, app_callback cb) {
  if (cb) {
    callback_connection = cb;
  }
  cmd_wifi_handle_credentials(ssid, pass);
  printf("ssid: %s\n", ssid);
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

  connect_args.index =
      arg_str1(NULL, NULL, "<index>", "Index of AP to connect");
  connect_args.end = arg_end(1);

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

  esp_console_cmd_t connect_cmd = {.command = "connect",
                                   .help = "Connect to a saved WiFi AP",
                                   .hint = NULL,
                                   .func = &cmd_wifi_connect_index,
                                   .argtable = &connect_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&join_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&save_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&show_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&delete_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&connect_cmd));
}
