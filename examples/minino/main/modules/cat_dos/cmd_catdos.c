/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Console example — various system commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "catdos_module.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#ifdef CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
  #define WITH_TASKS_INFO 1
#endif

static const char* TAG = "cmd_catdos";

static void cmd_catdos_register_catdos_web(void);
static void cmd_catdos_register_catdos_attack(void);

void register_catdos_commands(void) {
  cmd_catdos_register_catdos_web();
  cmd_catdos_register_catdos_attack();
}

static struct {
  struct arg_str* host;
  struct arg_str* port;
  struct arg_str* endpoint;
  struct arg_end* end;
} cmd_catdos_web_args;

static int cmd_catdos_web_set_config(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &cmd_catdos_web_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, cmd_catdos_web_args.end, argv[0]);
    return 1;
  }
  assert(cmd_catdos_web_args.host->count == 1);
  assert(cmd_catdos_web_args.port->count == 1);
  assert(cmd_catdos_web_args.endpoint->count == 1);
  const char* host = cmd_catdos_web_args.host->sval[0];
  const char* port = cmd_catdos_web_args.port->sval[0];
  const char* endpoint = cmd_catdos_web_args.endpoint->sval[0];

  catdos_module_set_target(host, port, endpoint);
  return 0;
}

// static void cmd_catdos_register_wifi_connect(void) {
//   connect_args.ssid =
//       arg_str1(NULL, NULL, "<ssid>", "SSID of AP to connect.");
//   connect_args.password =
//       arg_str1(NULL, NULL, "<pass>", "Password of AP to connect.");
//   connect_args.end = arg_end(2);

//   const esp_console_cmd_t cmd = {.command = "wifi_connect",
//                                  .help = "Connect to an AP",
//                                  .hint = NULL,
//                                  .func = &cmd_catdos_wifi_set_config,
//                                  .argtable = &connect_args};
//   ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
// }

static void cmd_catdos_register_catdos_web(void) {
  cmd_catdos_web_args.host =
      arg_str1(NULL, NULL, "<host>", "IP address of the target.");
  cmd_catdos_web_args.port =
      arg_str1(NULL, NULL, "<port>", "Port of the target.");
  cmd_catdos_web_args.endpoint =
      arg_str1(NULL, NULL, "<endpoint>", "Endpoint of the target.");
  cmd_catdos_web_args.end = arg_end(2);

  const esp_console_cmd_t cmd = {.command = "web_config",
                                 .help = "Configure the web target",
                                 .hint = NULL,
                                 .func = &cmd_catdos_web_set_config,
                                 .argtable = &cmd_catdos_web_args};
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void cmd_catdos_register_catdos_attack(void) {
  const esp_console_cmd_t cmd = {.command = "catdos",
                                 .help = "Send DOS to the web target",
                                 .hint = NULL,
                                 .func = &catdos_module_send_attack,
                                 .argtable = NULL};
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
