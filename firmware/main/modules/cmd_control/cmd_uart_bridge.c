#include "argtable3/argtable3.h"
#include "cmd_control.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "menus_module.h"
#include "uart_bridge.h"

static const char* TAG = "cmd_uart_bridge";

static struct {
  struct arg_str* message;
  struct arg_end* end;
} message_args;

static struct {
  struct arg_str* buffer_size;
  struct arg_int* timeout_ms;
  struct arg_end* end;
} get_messages_args;

// Structure to hold parameters for get_messages_task
typedef struct {
  const char* buffer_size;
  int timeout_ms;
} get_messages_params_t;

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

static void get_messages_task(void* args) {
  get_messages_params_t* params = (get_messages_params_t*) args;
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
  }
  free(params);
}

static void get_messages(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &get_messages_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, get_messages_args.end, argv[0]);
    return;
  }
  assert(get_messages_args.buffer_size->count == 1);
  assert(get_messages_args.timeout_ms->count == 1);
  const char* buffer_size = get_messages_args.buffer_size->sval[0];
  const int timeout_ms = *get_messages_args.timeout_ms->ival;

  get_messages_params_t* params = malloc(sizeof(get_messages_params_t));
  params->buffer_size = buffer_size;
  params->timeout_ms = timeout_ms;

  xTaskCreate(get_messages_task, "get_messages_task", 4096, (void*) params, 15,
              NULL);
}

void register_uart_bridge_commands() {
  message_args.message = arg_str1(NULL, NULL, "<message>", "Message to send");
  message_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_print_cmd = {
      .command = "print",
      .help = "Send a message over external UART pins on the MININO",
      .hint = NULL,
      .func = &send_message,
      .argtable = &message_args};

  get_messages_args.buffer_size =
      arg_str1(NULL, NULL, "<buffer_size>",
               "Size in bytes of the buffer to read data into");
  get_messages_args.timeout_ms = arg_int1(
      NULL, NULL, "<timeout_ms>", "Timeout in milliseconds for reading data");
  get_messages_args.end = arg_end(2);

  esp_console_cmd_t uart_bridge_get_messages_cmd = {
      .command = "get_messages",
      .help = "Get messages from external UART pins on the MININO",
      .hint = NULL,
      .func = &get_messages,
      .argtable = &get_messages_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_get_messages_cmd));
}
