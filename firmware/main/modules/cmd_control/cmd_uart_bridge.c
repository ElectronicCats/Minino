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

static struct {
  struct arg_int* buffer_size;
  struct arg_int* baud_rate;
  struct arg_int* data_bits;
  struct arg_int* parity;
  struct arg_int* stop_bits;
  struct arg_int* flow_ctrl;
  struct arg_end* end;
} uart_bridge_config_args;

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

uint8_t print_uart_bridge_config() {
  uart_bridge_config_t config = uart_bridge_get_config();
  printf("UART bridge configuration:\n");
  printf("\tBuffer size: %d\n", config.buffer_size);
  printf("\tBaud rate: %d\n", config.uart_config.baud_rate);
  printf("\tData bits: %d", config.uart_config.data_bits);
  switch (config.uart_config.data_bits) {
    case UART_DATA_5_BITS:
      printf(" (5 bits)\n");
      break;
    case UART_DATA_6_BITS:
      printf(" (6 bits)\n");
      break;
    case UART_DATA_7_BITS:
      printf(" (7 bits)\n");
      break;
    case UART_DATA_8_BITS:
      printf(" (8 bits)\n");
      break;
    default:
      printf(" (invalid)\n");
      break;
  }
  printf("\tParity: %d", config.uart_config.parity);
  switch (config.uart_config.parity) {
    case UART_PARITY_DISABLE:
      printf(" (disable)\n");
      break;
    case UART_PARITY_ODD:
      printf(" (odd)\n");
      break;
    case UART_PARITY_EVEN:
      printf(" (even)\n");
      break;
    default:
      printf(" (invalid)\n");
      break;
  }
  printf("\tStop bits: %d", config.uart_config.stop_bits);
  switch (config.uart_config.stop_bits) {
    case UART_STOP_BITS_1:
      printf(" (1 bit)\n");
      break;
    case UART_STOP_BITS_1_5:
      printf(" (1.5 bits)\n");
      break;
    case UART_STOP_BITS_2:
      printf(" (2 bits)\n");
      break;
    default:
      printf(" (invalid)\n");
      break;
  }
  printf("\tFlow control: %d", config.uart_config.flow_ctrl);
  switch (config.uart_config.flow_ctrl) {
    case UART_HW_FLOWCTRL_DISABLE:
      printf(" (disable)\n");
      break;
    case UART_HW_FLOWCTRL_RTS:
      printf(" (enable RX hardware flow control (rts))\n");
      break;
    case UART_HW_FLOWCTRL_CTS:
      printf(" (enable TX hardware flow control (cts))\n");
      break;
    case UART_HW_FLOWCTRL_CTS_RTS:
      printf(" (enable hardware flow control)\n");
      break;
    default:
      printf(" (invalid)\n");
      break;
  }
  return 0;
}

uint8_t uart_bridge_set_config(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &uart_bridge_config_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_args.end, argv[0]);
    return 1;
  }

  assert(uart_bridge_config_args.buffer_size->count == 1);
  assert(uart_bridge_config_args.baud_rate->count == 1);
  assert(uart_bridge_config_args.data_bits->count == 1);
  assert(uart_bridge_config_args.parity->count == 1);
  assert(uart_bridge_config_args.stop_bits->count == 1);
  assert(uart_bridge_config_args.flow_ctrl->count == 1);

  const int buffer_size = *uart_bridge_config_args.buffer_size->ival;
  const int baud_rate = *uart_bridge_config_args.baud_rate->ival;
  const int data_bits = *uart_bridge_config_args.data_bits->ival;
  const int parity = *uart_bridge_config_args.parity->ival;
  const int stop_bits = *uart_bridge_config_args.stop_bits->ival;
  const int flow_ctrl = *uart_bridge_config_args.flow_ctrl->ival;

  uart_config_t uart_config = {
      .baud_rate = baud_rate,
      .data_bits = data_bits,
      .parity = parity,
      .stop_bits = stop_bits,
      .flow_ctrl = flow_ctrl,
  };

  uart_bridge_config_t config = {
      .buffer_size = buffer_size,
      .uart_config = uart_config,
  };

  esp_err_t err = uart_bridge_begin(uart_config, buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge configuration (error code: %d)",
             err);
    return 1;
  }

  return 0;
}

void cmd_control_register_uart_bridge_commands() {
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

  esp_console_cmd_t uart_bridge_print_config_cmd = {
      .command = "uart_bridge_get_config",
      .help = "Print the UART bridge configuration",
      .hint = NULL,
      .func = &print_uart_bridge_config,
      .argtable = NULL};

  uart_bridge_config_args.buffer_size =
      arg_int1(NULL, NULL, "<buffer_size>",
               "Size in bytes of the buffer to read data into\n"
               "\tExample: 1024");
  uart_bridge_config_args.baud_rate =
      arg_int1(NULL, NULL, "<baud_rate>",
               "Baud rate\n"
               "\tFree to choose, but common values are:\n"
               "\t9600, 115200, 230400, 460800, 921600");
  uart_bridge_config_args.data_bits = arg_int1(NULL, NULL, "<data_bits>",
                                               "Data bits\n"
                                               "\tOptions:\n"
                                               "\t0: 5 bits\n"
                                               "\t1: 6 bits\n"
                                               "\t2: 7 bits\n"
                                               "\t3: 8 bits");
  uart_bridge_config_args.parity = arg_int1(NULL, NULL, "<parity>",
                                            "Parity\n"
                                            "\tOptions:\n"
                                            "\t0: disable\n"
                                            "\t2: odd\n"
                                            "\t3: even");
  uart_bridge_config_args.stop_bits = arg_int1(NULL, NULL, "<stop_bits>",
                                               "Stop bits\n"
                                               "\tOptions:\n"
                                               "\t1: 1 bit\n"
                                               "\t2: 1.5 bits\n"
                                               "\t3: 2 bits");
  uart_bridge_config_args.flow_ctrl =
      arg_int1(NULL, NULL, "<flow_ctrl>",
               "Flow control\n"
               "\tOptions:\n"
               "\t0: disable\n"
               "\t1: enable RX hardware flow control (rts)\n"
               "\t2: enable TX hardware flow control (cts)\n"
               "\t3: enable hardware flow control");
  uart_bridge_config_args.end = arg_end(6);

  esp_console_cmd_t uart_bridge_set_config_cmd = {
      .command = "uart_bridge_set_config",
      .help = "Set the UART bridge configuration",
      .hint = NULL,
      .func = &uart_bridge_set_config,
      .argtable = &uart_bridge_config_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_uart_bridge_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_config_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_config_cmd));
}
