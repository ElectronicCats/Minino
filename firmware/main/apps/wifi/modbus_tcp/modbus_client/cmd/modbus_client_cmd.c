#include "modbus_client_cmd.h"

#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "Modbus Client CMD"

#include "modbus_engine.h"

static struct {
  struct arg_str* function;
  struct arg_int* unit_id;
  struct arg_int* start_address;
  struct arg_int* values;
  struct arg_end* end;
} modbus_write_args;

static uint8_t get_write_function_code(const char* func_str) {
  if (strcmp(func_str, "coils") == 0)
    return 0x0F;
  if (strcmp(func_str, "registers") == 0)
    return 0x10;
  return 0xFF;
}

static int cmd_modbus_write(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &modbus_write_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, modbus_write_args.end, argv[0]);
    return 1;
  }

  uint8_t function_code =
      get_write_function_code(modbus_write_args.function->sval[0]);
  if (function_code == 0xFF) {
    ESP_LOGE(TAG, "Invalid function. Use 'coils' or 'registers'.\n");
    return 1;
  }

  uint8_t unit_id = modbus_write_args.unit_id->ival[0];
  uint16_t address = modbus_write_args.start_address->ival[0];
  int num_values = modbus_write_args.values->count;

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
      uint16_t val = modbus_write_args.values->ival[i];
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
          if (modbus_write_args.values->ival[bit_idx])
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

  // modbus_dos_send_pkt(request, idx);
  modbus_engine_set_request(request, idx);
  if (!modbus_engine_connect()) {
    ESP_LOGE(TAG, "Server Unreachable");
    return 1;
  }
  modbus_engine_send_request();
  // modbus_engine_disconnect();

  return 0;
}

void modbus_client_cmd_register_cmds() {
  modbus_write_args.function = arg_str1(
      NULL, NULL, "<func>", "Function: write_registers or write_coils");
  modbus_write_args.unit_id =
      arg_int1(NULL, NULL, "<unit_id>", "Modbus device ID");
  modbus_write_args.start_address =
      arg_int1(NULL, NULL, "<address>", "Start address");
  modbus_write_args.values =
      arg_intn(NULL, NULL, "<val> ...", 1, 128, "Values to write");
  modbus_write_args.end = arg_end(4);

  const esp_console_cmd_t cmd = {
      .command = "modbus_write",
      .help = "Generates a Modbus TCP request for multiple writes",
      .hint = NULL,
      .func = &cmd_modbus_write,
      .argtable = &modbus_write_args};
  ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
