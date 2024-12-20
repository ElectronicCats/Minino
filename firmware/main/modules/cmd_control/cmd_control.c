#include "cmd_control.h"
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"

static struct {
  struct arg_str* app;
  struct arg_end* end;
} message_args;

static void launch_app_task(char* entry_cmd) {
  menus_module_set_menu_over_cmd(entry_cmd);
  vTaskDelete(NULL);
}

static int launch_app(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &message_args);
  xTaskCreate(launch_app_task, "launch_app_task", 4096,
              message_args.app->sval[0], 15, NULL);
  return 0;
}

void cmd_control_register_launch_cmd_() {
  message_args.app = arg_str1(NULL, NULL, "<app>", "Name of the app to launch");
  message_args.end = arg_end(1);

  esp_console_cmd_t launch_cmd = {.command = "launch",
                                  .help = "Launch an app",
                                  .hint = NULL,
                                  .func = &launch_app,
                                  .argtable = &message_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&launch_cmd));
}
