#include "argtable3/argtable3.h"
#include "cat_console.h"
#include "cmd_control.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "uart_bridge.h"

static const char* TAG = "cmd_uart_bridge";

static bool run_uart_bridge_task = false;

static struct {
  struct arg_str* message;
  struct arg_end* end;
} message_args;

static struct {
  struct arg_str* buffer_size;
  struct arg_int* timeout_ms;
  struct arg_end* end;
} uart_bridge_args;

// Structure to hold parameters for uart_bridge_task
typedef struct {
  const char* buffer_size;
  int timeout_ms;
} uart_bridge_params_t;

void ctrl_c_callback() {
  run_uart_bridge_task = false;
}

static void send_message(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &message_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, message_args.end, argv[0]);
    return;
  }
  assert(message_args.message->count == 1);
  const char* message = message_args.message->sval[0];

  uart_bridge_write(message, strlen(message));
}

static void uart_bridge_task(void* args) {
  if (run_uart_bridge_task) {
    ESP_LOGI(TAG, "uart_bridge_task already running");
    return;
  }

  run_uart_bridge_task = true;
  uart_bridge_params_t* params = (uart_bridge_params_t*) args;
  const char* buffer_size = params->buffer_size;
  const int timeout_ms = params->timeout_ms;

  ESP_LOGI(TAG, "Buffer size: %s", buffer_size);
  ESP_LOGI(TAG, "Timeout: %d", timeout_ms);

  char buffer[atoi(buffer_size)];
  while (true) {
    esp_err_t err = uart_bridge_read(buffer, sizeof(buffer), timeout_ms);
    if (err == ESP_OK) {
      printf("Received: %s\n", buffer);

      // Clear buffer
      memset(buffer, 0, sizeof(buffer));
    }

    if (!run_uart_bridge_task) {
      break;
    }
  }
  free(params);
  ESP_LOGI(TAG, "uart_bridge_task ended :)");
  vTaskDelete(NULL);
}

static void uart_bridge(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &uart_bridge_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_args.end, argv[0]);
    return;
  }

  assert(uart_bridge_args.buffer_size->count == 1);
  assert(uart_bridge_args.timeout_ms->count == 1);
  const char* buffer_size = uart_bridge_args.buffer_size->sval[0];
  const int timeout_ms = *uart_bridge_args.timeout_ms->ival;

  uart_bridge_params_t* params = malloc(sizeof(uart_bridge_params_t));
  params->buffer_size = buffer_size;
  params->timeout_ms = timeout_ms;

  cat_console_register_ctrl_c_handler(&ctrl_c_callback);
  ESP_LOGI(TAG, "Starting uart_bridge_task");
  xTaskCreate(uart_bridge_task, "uart_bridge_task", 4096, (void*) params, 15,
              NULL);
  ESP_LOGI(TAG, "uart_bridge_task started");
}

void register_uart_bridge_commands() {
#if !defined(CONFIG_CMD_UART_BRIDGE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  message_args.message = arg_str1(NULL, NULL, "<message>",
                                  "Message to send\n\n"
                                  "\tExample: print hello\n"
                                  "\tExample: print \"hello world\"");
  message_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_print_cmd = {
      .command = "print",
      .help = "Send a message over external UART TXD pin on the MININO",
      .hint = NULL,
      .func = &send_message,
      .argtable = &message_args};

  uart_bridge_args.buffer_size =
      arg_str1(NULL, NULL, "<buffer_size>",
               "Size in bytes of the buffer to read data into");
  uart_bridge_args.timeout_ms =
      arg_int1(NULL, NULL, "<timeout_ms>",
               "Timeout in milliseconds for reading data\n\n"
               "\tExample: uart_bridge 128 1000");
  uart_bridge_args.end = arg_end(2);

  esp_console_cmd_t uart_bridge_uart_bridge_cmd = {
      .command = "uart_bridge",
      .help =
          "Get messages from external UART RXD pin on the MININO.\n"
          "Press Ctrl+C to stop reading messages.",
      .hint = NULL,
      .func = &uart_bridge,
      .argtable = &uart_bridge_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_uart_bridge_cmd));
}
