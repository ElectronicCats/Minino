#include "menus_module_cmd.h"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"

//////////////////////////   LAUNCH   ////////////////////

static struct {
  struct arg_str* app;
  struct arg_end* end;
} launch_args;

static void launch_app_task(char* entry_cmd) {
  menus_module_set_menu_over_cmd(entry_cmd);
  vTaskDelete(NULL);
}

static int launch_app(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &launch_args);
  xTaskCreate(launch_app_task, "launch_app_task", 4096,
              launch_args.app->sval[0], 15, NULL);
  return 0;
}

static void register_launch_cmd() {
  launch_args.app = arg_str1(NULL, NULL, "<app>",
                             "Name of the app to LAUNCH\n"
                             "analyzer\n"
                             "deauth\n"
                             "deauth_scan\n"
                             "dos\n"
                             "ssid_spam\n"
                             "trakers_scan\n"
                             "spam\n"
                             "hid\n"
                             "adv\n"
                             "switch\n"
                             "light\n"
                             "zigbee_sniffer\n"
                             "broadcast\n"
                             "thread_sniffer\n"
                             "thread_sniffer_run\n"
                             "i2c_scanner\n"
                             "uart_bridge\n"
                             "file_manager\n"
                             "ota\n"
                             "display_config_module_begin\n"
                             "logs_output\n"
                             "wifi_settings_begin\n"
                             "update_sd_card_info\n"
                             "sd_card_settings_verify_sd_card\n"
                             "stealth_mode_open_menu\n"
                             "sleep_mode_settings"

                             "\n\n"
                             "Example: launch analyzer\n"
                             "Example: launch i2c_scanner");
  launch_args.end = arg_end(1);

  esp_console_cmd_t launch_cmd = {.command = "launch",
                                  .help = "Launch an app",
                                  .category = "Minino",
                                  .hint = NULL,
                                  .func = &launch_app,
                                  .argtable = &launch_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&launch_cmd));
}

//////////////////////////   EXIT   ////////////////////

static int exit_app(int argc, char** argv) {
  menus_module_restart();
  return 0;
}

static void register_exit_cmd() {
  esp_console_cmd_t exit_cmd = {.command = "exit_app",
                                .help = "Exits the current app",
                                .category = "Minino",
                                .hint = NULL,
                                .func = &exit_app,
                                .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&exit_cmd));
}

void menus_module_cmd_register_cmds() {
  register_launch_cmd();
  register_exit_cmd();
}
