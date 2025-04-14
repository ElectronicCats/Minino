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
#include "wifi_ap_manager.h"

// #define JOIN_TIMEOUT_MS (5000)
// #define MAXIMUM_RETRY   (3)

static const char* TAG = "cmd_wifi";

// static EventGroupHandle_t wifi_event_group;
// const int CONNECTED_BIT = BIT0;
// static app_callback callback_connection;
static bool disconnect_cb = false;
static int reconnections = 0;
static int connect(int argc, char** argv);
static bool wifi_join(const char* ssid, const char* pass, int timeout_ms);
static void cmd_wifi_handle_credentials(const char* ssid, const char* pass);
static int cmd_wifi_show_aps(int argc, char** argv);

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
    ESP_LOGW(__func__, "Error parsing arguments");
    printf("Error parsing arguments\n");
    arg_print_errors(stderr, connect_args.end, argv[0]);
    return 1;
  }
  int count = preferences_get_int("count_ap", 0);
  int index = atoi(connect_args.index->sval[0]);
  if (index > count) {
    ESP_LOGW(__func__, "Error parsing arguments");
    printf("Error parsing arguments\n");
    arg_print_errors(stderr, connect_args.end, argv[0]);
    return 1;
  }
  return wifi_ap_manager_connect_index(index);
}

static int cmd_wifi_save_credentials(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &join_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, join_args.end, argv[0]);
    ESP_LOGW(__func__, "Error parsing arguments");
    return -1;
  }
  cmd_wifi_handle_credentials(join_args.ssid->sval[0],
                              join_args.password->sval[0]);
  ESP_LOGI(__func__, "Credentials saved: %s:%s\n", join_args.ssid->sval[0],
           join_args.password->sval[0]);
  printf("Credentials saved: %s\n", join_args.ssid->sval[0]);
  return 0;
}

static int cmd_wifi_delete_crendentials(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &delete_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, delete_args.end, argv[0]);
    ESP_LOGW(__func__, "Error parsing arguments");
    return 1;
  }
  int index = atoi(delete_args.index->sval[0]);
  int count = preferences_get_int("count_ap", 0);
  if (index >= count) {
    ESP_LOGW(__func__, "Index out of range");
    printf("Index out of range\n");
    return 1;
  }

  char* wifi_list[count - 1];
  int new_counter = 0;

  for (int i = 0; i < count; i++) {
    char wifi_apk[100];
    char wifi_ssid[100];

    sprintf(wifi_apk, "wifi%d", i);
    if (i == index) {
      preferences_get_string(wifi_apk, wifi_ssid, 100);
      preferences_remove(wifi_ssid);
      preferences_remove(wifi_apk);
    } else {
      preferences_get_string(wifi_apk, wifi_ssid, 100);
      wifi_list[new_counter] = malloc(sizeof(wifi_ssid));
      wifi_list[new_counter] = strdup(wifi_ssid);
      new_counter++;
    }
  }
  for (int i = 0; i < new_counter; i++) {
    char wifi_apk[100];
    sprintf(wifi_apk, "wifi%d", i);
    preferences_put_string(wifi_apk, wifi_list[i]);
    free(wifi_list[i]);
  }

  preferences_put_int("count_ap", new_counter);
  return 0;
}

static void cmd_wifi_handle_credentials(const char* ssid, const char* pass) {
  ESP_LOGI(__func__, "Saving credentials");
  int get_count = preferences_get_int("count_ap", 0);
  preferences_put_int("count_ap", get_count + 1);
  char wifi_ap[100];
  sprintf(wifi_ap, "wifi%d", get_count);
  preferences_put_string(wifi_ap, ssid);
  preferences_put_string(ssid, pass);
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

  esp_console_cmd_t save_cmd = {.command = "save",
                                .help = "Save WiFi AP credentials and join",
                                .category = "wifi",
                                .hint = NULL,
                                .func = &cmd_wifi_save_credentials,
                                .argtable = &join_args};

  esp_console_cmd_t show_cmd = {.command = "list",
                                .help = "Show saved WiFi AP credentials",
                                .category = "wifi",
                                .hint = NULL,
                                .func = &cmd_wifi_show_aps,
                                .argtable = NULL};

  esp_console_cmd_t delete_cmd = {.command = "delete",
                                  .help = "Delete saved WiFi AP credentials",
                                  .category = "wifi",
                                  .hint = NULL,
                                  .func = &cmd_wifi_delete_crendentials,
                                  .argtable = &delete_args};

  esp_console_cmd_t connect_cmd = {.command = "connect",
                                   .help = "Connect to a saved WiFi AP",
                                   .category = "wifi",
                                   .hint = NULL,
                                   .func = &cmd_wifi_connect_index,
                                   .argtable = &connect_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&save_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&show_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&delete_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&connect_cmd));
}
