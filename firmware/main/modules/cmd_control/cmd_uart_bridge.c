#include "argtable3/argtable3.h"
#include "cmd_control.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "uart_bridge.h"

static struct {
  struct arg_str* message;
  struct arg_end* end;
} launch_args;

static void send_message(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &launch_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, launch_args.end, argv[0]);
    return;
  }
  assert(launch_args.message->count == 1);
  const char* message = launch_args.message->sval[0];

  uart_bridge_write(message, strlen(message));
}

void register_uart_bridge_commands() {
  launch_args.message = arg_str1(NULL, NULL, "<message>", "Message to send");
  launch_args.end = arg_end(1);

  esp_console_cmd_t launch_cmd = {
      .command = "print",
      .help = "Send a message over external UART pins on the MININO",
      .hint = NULL,
      .func = &send_message,
      .argtable = &launch_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&launch_cmd));
}
