#include <stdlib.h>
#include <string.h>
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
static char* category = "GPIO";
static bool run_uart_bridge_task = false;

static struct {
  struct arg_str* message;
  struct arg_end* end;
} print_args;

static struct {
  struct arg_str* message;
  struct arg_end* end;
} println_args;

static struct {
  struct arg_int* buffer_size;
  struct arg_int* baud_rate;
  struct arg_int* data_bits;
  struct arg_int* parity;
  struct arg_int* stop_bits;
  struct arg_int* flow_ctrl;
  struct arg_end* end;
} uart_bridge_config_args;

static struct {
  struct arg_int* buffer_size;
  struct arg_end* end;
} uart_bridge_config_buffer_size_args;

static struct {
  struct arg_int* baud_rate;
  struct arg_end* end;
} uart_bridge_config_baud_rate_args;

static struct {
  struct arg_int* data_bits;
  struct arg_end* end;
} uart_bridge_config_data_bits_args;

static struct {
  struct arg_int* parity;
  struct arg_end* end;
} uart_bridge_config_parity_args;

static struct {
  struct arg_int* stop_bits;
  struct arg_end* end;
} uart_bridge_config_stop_bits_args;

static struct {
  struct arg_int* flow_ctrl;
  struct arg_end* end;
} uart_bridge_config_flow_ctrl_args;

// Structure to hold parameters for uart_bridge_task
typedef struct {
  int buffer_size;
  int timeout_ms;
} uart_bridge_params_t;

void ctrl_c_callback() {
  run_uart_bridge_task = false;
}

#define UART_BRIDGE_MAX_BUFFER_SIZE 4096

static int uart_bridge_print(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &print_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, print_args.end, argv ? argv[0] : "print");
    return 1;
  }
  if (print_args.message->count != 1 || print_args.message->sval[0] == NULL) {
    return 1;
  }
  const char* message = print_args.message->sval[0];

  uart_bridge_write(message, strlen(message));
  return 0;
}

static int uart_bridge_println(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &println_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, println_args.end, argv ? argv[0] : "println");
    return 1;
  }
  if (println_args.message->count != 1 || println_args.message->sval[0] == NULL) {
    return 1;
  }
  const char* message = println_args.message->sval[0];

  uart_bridge_write(message, strlen(message));
  uart_bridge_write("\n", 1);
  return 0;
}

static void uart_bridge_task(void* args) {
  if (args == NULL) {
    ESP_LOGE(TAG, "uart_bridge_task received NULL params");
    vTaskDelete(NULL);
    return;
  }
  if (run_uart_bridge_task) {
    ESP_LOGI(TAG, "uart_bridge_task already running");
    free(args);
    return;
  }

  run_uart_bridge_task = true;
  uart_bridge_params_t* params = (uart_bridge_params_t*) args;
  int buffer_size = params->buffer_size;
  const int timeout_ms = params->timeout_ms;

  if (buffer_size <= 0 || buffer_size > UART_BRIDGE_MAX_BUFFER_SIZE) {
    buffer_size = 1024;
  }

  ESP_LOGI(TAG, "Buffer size: %d", buffer_size);
  ESP_LOGI(TAG, "Timeout: %d", timeout_ms);

  char* buffer = (char*) malloc(buffer_size + 1);
  if (buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for uart buffer");
    free(params);
    run_uart_bridge_task = false;
    vTaskDelete(NULL);
    return;
  }

  while (true) {
    memset(buffer, 0, buffer_size + 1);
    esp_err_t err = uart_bridge_read(buffer, buffer_size, timeout_ms);
    if (err == ESP_OK) {
      buffer[buffer_size] = '\0';
      printf("Received: %s\n", buffer);
    }

    if (!run_uart_bridge_task) {
      break;
    }
  }
  free(buffer);
  free(params);
  ESP_LOGI(TAG, "uart_bridge_task ended :)");
  vTaskDelete(NULL);
}

static int uart_bridge(int argc, char** argv) {
  (void) argc;
  (void) argv;
  // TODO: Add a command to change these values
  const int buffer_size = 1024;
  const int timeout_ms = 100;

  uart_bridge_params_t* params = malloc(sizeof(uart_bridge_params_t));
  if (params == NULL) {
    ESP_LOGE(TAG, "Failed to allocate params for uart_bridge");
    return 1;
  }
  params->buffer_size = buffer_size;
  params->timeout_ms = timeout_ms;

  cat_console_register_ctrl_c_handler(&ctrl_c_callback);
  ESP_LOGI(TAG, "Starting uart_bridge_task");
  BaseType_t ret = xTaskCreate(uart_bridge_task, "uart_bridge_task", 4096,
                               (void*) params, 15, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create uart_bridge_task");
    free(params);
    return 1;
  }
  ESP_LOGI(TAG, "uart_bridge_task started");
  return 0;
}

int print_uart_bridge_config(int argc, char** argv) {
  (void) argc;
  (void) argv;
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

int uart_bridge_set_config(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &uart_bridge_config_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_args.end,
                     argv ? argv[0] : "uart_bridge_set_config");
    return 1;
  }

  if (uart_bridge_config_args.buffer_size->count != 1 ||
      uart_bridge_config_args.baud_rate->count != 1 ||
      uart_bridge_config_args.data_bits->count != 1 ||
      uart_bridge_config_args.parity->count != 1 ||
      uart_bridge_config_args.stop_bits->count != 1 ||
      uart_bridge_config_args.flow_ctrl->count != 1) {
    printf("Error: Missing required configuration parameters\n");
    return 1;
  }

  const int buffer_size = *uart_bridge_config_args.buffer_size->ival;
  const int baud_rate = *uart_bridge_config_args.baud_rate->ival;
  const int data_bits = *uart_bridge_config_args.data_bits->ival;
  const int parity = *uart_bridge_config_args.parity->ival;
  const int stop_bits = *uart_bridge_config_args.stop_bits->ival;
  const int flow_ctrl = *uart_bridge_config_args.flow_ctrl->ival;

  if (buffer_size <= 0 || buffer_size > UART_BRIDGE_MAX_BUFFER_SIZE) {
    printf("Error: buffer_size must be between 1 and %d\n",
           UART_BRIDGE_MAX_BUFFER_SIZE);
    return 1;
  }
  if (baud_rate <= 0 || baud_rate > 5000000) {
    printf("Error: baud_rate must be positive (e.g. 115200)\n");
    return 1;
  }
  if (data_bits < 0 || data_bits > 3) {
    printf("Error: data_bits must be between 0 (5 bits) and 3 (8 bits)\n");
    return 1;
  }
  if (parity < 0 || parity > 3) {
    printf("Error: parity must be 0 (disable), 2 (odd), or 3 (even)\n");
    return 1;
  }
  if (stop_bits < 1 || stop_bits > 3) {
    printf("Error: stop_bits must be 1 (1 bit), 2 (1.5 bits), or 3 (2 bits)\n");
    return 1;
  }
  if (flow_ctrl < 0 || flow_ctrl > 3) {
    printf("Error: flow_ctrl must be between 0 and 3\n");
    return 1;
  }

  uart_config_t uart_config = {
      .baud_rate = baud_rate,
      .data_bits = data_bits,
      .parity = parity,
      .stop_bits = stop_bits,
      .flow_ctrl = flow_ctrl,
  };

  esp_err_t err = uart_bridge_begin(uart_config, buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge configuration (error code: %d)",
             err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_buffer_size(int argc, char** argv) {
  int nerrors =
      arg_parse(argc, argv, (void**) &uart_bridge_config_buffer_size_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_buffer_size_args.end,
                     argv ? argv[0] : "uart_bridge_set_buffer_size");
    return 1;
  }

  if (uart_bridge_config_buffer_size_args.buffer_size->count != 1 ||
      uart_bridge_config_buffer_size_args.buffer_size->ival == NULL) {
    return 1;
  }

  const int buffer_size =
      *uart_bridge_config_buffer_size_args.buffer_size->ival;

  if (buffer_size <= 0 || buffer_size > UART_BRIDGE_MAX_BUFFER_SIZE) {
    printf("Error: buffer_size must be between 1 and %d\n",
           UART_BRIDGE_MAX_BUFFER_SIZE);
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.buffer_size = buffer_size;

  esp_err_t err = uart_bridge_begin(config.uart_config, buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge buffer size (error code: %d)",
             err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_baud_rate(int argc, char** argv) {
  int nerrors =
      arg_parse(argc, argv, (void**) &uart_bridge_config_baud_rate_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_baud_rate_args.end,
                     argv ? argv[0] : "uart_bridge_set_baud_rate");
    return 1;
  }

  if (uart_bridge_config_baud_rate_args.baud_rate->count != 1 ||
      uart_bridge_config_baud_rate_args.baud_rate->ival == NULL) {
    return 1;
  }

  const int baud_rate = *uart_bridge_config_baud_rate_args.baud_rate->ival;
  if (baud_rate <= 0 || baud_rate > 5000000) {
    printf("Error: Invalid baud rate\n");
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.uart_config.baud_rate = baud_rate;

  esp_err_t err = uart_bridge_begin(config.uart_config, config.buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge baud rate (error code: %d)", err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_data_bits(int argc, char** argv) {
  int nerrors =
      arg_parse(argc, argv, (void**) &uart_bridge_config_data_bits_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_data_bits_args.end,
                     argv ? argv[0] : "uart_bridge_set_data_bits");
    return 1;
  }

  if (uart_bridge_config_data_bits_args.data_bits->count != 1 ||
      uart_bridge_config_data_bits_args.data_bits->ival == NULL) {
    return 1;
  }

  const int data_bits = *uart_bridge_config_data_bits_args.data_bits->ival;
  if (data_bits < 0 || data_bits > 3) {
    printf("Error: Invalid data bits (must be 0-3)\n");
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.uart_config.data_bits = data_bits;

  esp_err_t err = uart_bridge_begin(config.uart_config, config.buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge data bits (error code: %d)", err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_parity(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &uart_bridge_config_parity_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_parity_args.end,
                     argv ? argv[0] : "uart_bridge_set_parity");
    return 1;
  }

  if (uart_bridge_config_parity_args.parity->count != 1 ||
      uart_bridge_config_parity_args.parity->ival == NULL) {
    return 1;
  }

  const int parity = *uart_bridge_config_parity_args.parity->ival;
  if (parity < 0 || parity > 3) {
    printf("Error: Invalid parity\n");
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.uart_config.parity = parity;

  esp_err_t err = uart_bridge_begin(config.uart_config, config.buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge parity (error code: %d)", err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_stop_bits(int argc, char** argv) {
  int nerrors =
      arg_parse(argc, argv, (void**) &uart_bridge_config_stop_bits_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_stop_bits_args.end,
                     argv ? argv[0] : "uart_bridge_set_stop_bits");
    return 1;
  }

  if (uart_bridge_config_stop_bits_args.stop_bits->count != 1 ||
      uart_bridge_config_stop_bits_args.stop_bits->ival == NULL) {
    return 1;
  }

  const int stop_bits = *uart_bridge_config_stop_bits_args.stop_bits->ival;
  if (stop_bits < 1 || stop_bits > 3) {
    printf("Error: Invalid stop bits\n");
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.uart_config.stop_bits = stop_bits;

  esp_err_t err = uart_bridge_begin(config.uart_config, config.buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge stop bits (error code: %d)", err);
    return 1;
  }

  return 0;
}

static int uart_bridge_set_flow_ctrl(int argc, char** argv) {
  int nerrors =
      arg_parse(argc, argv, (void**) &uart_bridge_config_flow_ctrl_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, uart_bridge_config_flow_ctrl_args.end,
                     argv ? argv[0] : "uart_bridge_set_flow_ctrl");
    return 1;
  }

  if (uart_bridge_config_flow_ctrl_args.flow_ctrl->count != 1 ||
      uart_bridge_config_flow_ctrl_args.flow_ctrl->ival == NULL) {
    return 1;
  }

  const int flow_ctrl = *uart_bridge_config_flow_ctrl_args.flow_ctrl->ival;
  if (flow_ctrl < 0 || flow_ctrl > 3) {
    printf("Error: Invalid flow control\n");
    return 1;
  }

  uart_bridge_config_t config = uart_bridge_get_config();
  config.uart_config.flow_ctrl = flow_ctrl;

  esp_err_t err = uart_bridge_begin(config.uart_config, config.buffer_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART bridge flow control (error code: %d)",
             err);
    return 1;
  }

  return 0;
}

void cmd_control_register_uart_bridge_commands() {
#if !defined(CONFIG_CMD_UART_BRIDGE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  print_args.message = arg_str1(NULL, NULL, "<message>",
                                "Message to send\n\n"
                                "\tExample: print hello\n"
                                "\tExample: print \"hello world\"");
  print_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_print_cmd = {
      .command = "print",
      .help = "Send a message over external UART TXD pin on the MININO",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_print,
      .argtable = &print_args};

  println_args.message = arg_str1(NULL, NULL, "<message>",
                                  "Message to send\n\n"
                                  "\tExample: println hello\n"
                                  "\tExample: println \"hello world\"");
  println_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_println_cmd = {
      .command = "println",
      .help =
          "Send a message with a newline over external UART TXD pin on the "
          "MININO",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_println,
      .argtable = &println_args};

  esp_console_cmd_t uart_bridge_uart_bridge_cmd = {
      .command = "uart_bridge",
      .help =
          "Get messages from external UART RXD pin on the MININO.\n"
          "Press Ctrl+C to stop reading messages.\n",
      .category = category,
      .hint = NULL,
      .func = &uart_bridge,
      .argtable = NULL};

  esp_console_cmd_t uart_bridge_print_config_cmd = {
      .command = "uart_bridge_get_config",
      .help = "Print the UART bridge configuration",
      .hint = NULL,
      .category = category,
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
      .category = category,
      .func = &uart_bridge_set_config,
      .argtable = &uart_bridge_config_args};

  uart_bridge_config_buffer_size_args.buffer_size =
      arg_int1(NULL, NULL, "<buffer_size>",
               "Size in bytes of the buffer to read data into\n"
               "\tExample: 1024");
  uart_bridge_config_buffer_size_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_buffer_size_cmd = {
      .command = "uart_bridge_set_buffer_size",
      .help = "Set the buffer size of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_buffer_size,
      .argtable = &uart_bridge_config_buffer_size_args};

  uart_bridge_config_baud_rate_args.baud_rate =
      arg_int1(NULL, NULL, "<baud_rate>",
               "Baud rate\n"
               "\tFree to choose, but common values are:\n"
               "\t9600, 115200, 230400, 460800, 921600");
  uart_bridge_config_baud_rate_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_baud_rate_cmd = {
      .command = "uart_bridge_set_baud_rate",
      .help = "Set the baud rate of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_baud_rate,
      .argtable = &uart_bridge_config_baud_rate_args};

  uart_bridge_config_data_bits_args.data_bits =
      arg_int1(NULL, NULL, "<data_bits>",
               "Data bits\n"
               "\tOptions:\n"
               "\t0: 5 bits\n"
               "\t1: 6 bits\n"
               "\t2: 7 bits\n"
               "\t3: 8 bits");
  uart_bridge_config_data_bits_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_data_bits_cmd = {
      .command = "uart_bridge_set_data_bits",
      .help = "Set the data bits of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_data_bits,
      .argtable = &uart_bridge_config_data_bits_args};

  uart_bridge_config_parity_args.parity = arg_int1(NULL, NULL, "<parity>",
                                                   "Parity\n"
                                                   "\tOptions:\n"
                                                   "\t0: disable\n"
                                                   "\t2: odd\n"
                                                   "\t3: even");
  uart_bridge_config_parity_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_parity_cmd = {
      .command = "uart_bridge_set_parity",
      .help = "Set the parity of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_parity,
      .argtable = &uart_bridge_config_parity_args};

  uart_bridge_config_stop_bits_args.stop_bits =
      arg_int1(NULL, NULL, "<stop_bits>",
               "Stop bits\n"
               "\tOptions:\n"
               "\t1: 1 bit\n"
               "\t2: 1.5 bits\n"
               "\t3: 2 bits");
  uart_bridge_config_stop_bits_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_stop_bits_cmd = {
      .command = "uart_bridge_set_stop_bits",
      .help = "Set the stop bits of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_stop_bits,
      .argtable = &uart_bridge_config_stop_bits_args};

  uart_bridge_config_flow_ctrl_args.flow_ctrl =
      arg_int1(NULL, NULL, "<flow_ctrl>",
               "Flow control\n"
               "\tOptions:\n"
               "\t0: disable\n"
               "\t1: enable RX hardware flow control (rts)\n"
               "\t2: enable TX hardware flow control (cts)\n"
               "\t3: enable hardware flow control");
  uart_bridge_config_flow_ctrl_args.end = arg_end(1);

  esp_console_cmd_t uart_bridge_set_flow_ctrl_cmd = {
      .command = "uart_bridge_set_flow_ctrl",
      .help = "Set the flow control of the UART bridge",
      .hint = NULL,
      .category = category,
      .func = &uart_bridge_set_flow_ctrl,
      .argtable = &uart_bridge_config_flow_ctrl_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_println_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_uart_bridge_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_print_config_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_config_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_buffer_size_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_baud_rate_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_data_bits_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_parity_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_stop_bits_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&uart_bridge_set_flow_ctrl_cmd));
}
