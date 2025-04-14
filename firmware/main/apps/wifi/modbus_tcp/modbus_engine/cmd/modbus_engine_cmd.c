#include "modbus_engine_cmd.h"

#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "Modbus Engine CMD"

#include "modbus_engine.h"

static struct {
  struct arg_str* function;
  struct arg_int* unit_id;
  struct arg_int* start_address;
  struct arg_int* values;
  struct arg_end* end;
} mb_engine_set_req_args;

static struct {
  struct arg_str* ip;
  struct arg_int* port;
  struct arg_end* end;
} mb_engine_set_server_args;

static uint8_t get_write_function_code(const char* func_str) {
  if (strcmp(func_str, "coils") == 0)
    return 0x0F;
  if (strcmp(func_str, "registers") == 0)
    return 0x10;
  return 0xFF;
}

static int cmd_mb_engine_set_request(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_engine_set_req_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_engine_set_req_args.end, argv[0]);
    return 1;
  }

  uint8_t function_code =
      get_write_function_code(mb_engine_set_req_args.function->sval[0]);
  if (function_code == 0xFF) {
    ESP_LOGE(TAG, "Invalid function. Use 'coils' or 'registers'.\n");
    return 1;
  }

  uint8_t unit_id = mb_engine_set_req_args.unit_id->ival[0];
  uint16_t address = mb_engine_set_req_args.start_address->ival[0];
  int num_values = mb_engine_set_req_args.values->count;

  if (num_values == 0) {
    ESP_LOGE(TAG, "You must provide at least one value to write.\n");
    return 1;
  }

  uint8_t request[260] = {0};
  int idx = 0;

  // MBAP header
  request[idx++] = 0x00;  // Transaction ID
  request[idx++] = 0x01;
  request[idx++] = 0x00;  // Protocol ID
  request[idx++] = 0x00;

  uint16_t data_len = 7;
  uint8_t byte_count = 0;

  request[6] = unit_id;
  request[7] = function_code;
  request[8] = address >> 8;
  request[9] = address & 0xFF;

  if (function_code == 0x10) {
    request[10] = num_values >> 8;
    request[11] = num_values & 0xFF;
    byte_count = num_values * 2;
    request[12] = byte_count;
    idx = 13;

    for (int i = 0; i < num_values; i++) {
      uint16_t val = mb_engine_set_req_args.values->ival[i];
      request[idx++] = val >> 8;
      request[idx++] = val & 0xFF;
    }

    data_len += 6 + byte_count;

  } else if (function_code == 0x0F) {
    request[10] = num_values >> 8;
    request[11] = num_values & 0xFF;

    byte_count = (num_values + 7) / 8;
    request[12] = byte_count;
    idx = 13;

    for (int i = 0; i < byte_count; i++) {
      uint8_t b = 0;
      for (int j = 0; j < 8; j++) {
        int bit_idx = i * 8 + j;
        if (bit_idx < num_values) {
          if (mb_engine_set_req_args.values->ival[bit_idx])
            b |= (1 << j);
        }
      }
      request[idx++] = b;
    }

    data_len += 6 + byte_count;
  }

  request[4] = data_len >> 8;
  request[5] = data_len & 0xFF;

  ESP_LOGI(TAG, "Modbus write request generated:");
  for (int i = 0; i < idx; i++) {
    printf("%02X ", request[i]);
  }
  printf("\n");

  modbus_engine_set_request(request, idx);
  return 0;
}

static int cmd_mb_engine_set_server(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_engine_set_server_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_engine_set_server_args.end, argv[0]);
    return 1;
  }

  modbus_engine_set_server(mb_engine_set_server_args.ip->sval[0],
                           mb_engine_set_server_args.port->ival[0]);
  return 0;
}

static int cmd_mb_engine_connect(int argc, char** argv) {
  modbus_engine_connect();
  return 0;
}

static int cmd_mb_engine_disconnect(int argc, char** argv) {
  modbus_engine_disconnect();
  return 0;
}

static int cmd_mb_engine_send_req(int argc, char** argv) {
  modbus_engine_send_request();
  return 0;
}

static int cmd_mb_engine_print_status(int argc, char** argv) {
  modbus_engine_print_status();
  return 0;
}

void modbus_engine_cmd_register_cmds() {
  mb_engine_set_req_args.function = arg_str1(NULL, NULL, "<coils / registers>",
                                             "Function: registers or coils");
  mb_engine_set_req_args.unit_id =
      arg_int1(NULL, NULL, "<slave_id>", "Modbus device ID");
  mb_engine_set_req_args.start_address =
      arg_int1(NULL, NULL, "<address>", "Start address");
  mb_engine_set_req_args.values =
      arg_intn(NULL, NULL, "<val> ...", 1, 128, "Values to write");
  mb_engine_set_req_args.end = arg_end(4);

  const esp_console_cmd_t mb_set_req_cmd = {
      .command = "mb_engine_set_req",
      .help = "Generates a Modbus TCP request for multiple writes",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_set_request,
      .argtable = &mb_engine_set_req_args};

  mb_engine_set_server_args.ip =
      arg_str1(NULL, NULL, "<ip>", "Server ip Addr. Ex: 192.168.100.100");
  mb_engine_set_server_args.port =
      arg_int1(NULL, NULL, "<port>", "Server Port, Ex: 502");
  mb_engine_set_server_args.end = arg_end(3);

  const esp_console_cmd_t mb_set_server_cmd = {
      .command = "mb_engine_set_server",
      .help = "Setup the target Modbus Server",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_set_server,
      .argtable = &mb_engine_set_server_args};

  const esp_console_cmd_t mb_connect_cmd = {
      .command = "mb_engine_connect",
      .help = "It connects Minino to the Modbus Server",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_connect,
      .argtable = NULL};

  const esp_console_cmd_t mb_disconnect_cmd = {
      .command = "mb_engine_disconnect",
      .help = "It disconnects Minino from the Modbus Server",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_disconnect,
      .argtable = NULL};

  const esp_console_cmd_t mb_send_req_cmd = {
      .command = "mb_engine_send_req",
      .help = "Sends a request to the Modbus Server",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_send_req,
      .argtable = NULL};

  const esp_console_cmd_t mb_print_status_cmd = {
      .command = "mb_engine_status",
      .help = "Prints the modbus engine Status",
      .hint = NULL,
      .category = "Modbus",
      .func = &cmd_mb_engine_print_status,
      .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_set_req_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_set_server_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_connect_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_send_req_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_print_status_cmd));
}
