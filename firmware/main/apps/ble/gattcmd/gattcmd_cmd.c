#include <string.h>
#include "argtable3/argtable3.h"
#include "cat_console.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gattcmd_module.h"

#define GATTCMD_CMD_NAME "gattcmd_set_client"

static struct {
  struct arg_str* remote_addr;
  struct arg_end* end;
} gattccmd_client_args;

static struct {
  struct arg_str* addr;
  struct arg_str* gatt;
  struct arg_str* value;
  struct arg_end* end;
} gattccmd_write_args;

static int gattccmd_enum_client(int argc, char** argv) {
  int nerrros = arg_parse(argc, argv, (void**) &gattccmd_client_args);
  if (nerrros != 0) {
    arg_print_errors(stderr, gattccmd_client_args.end, GATTCMD_CMD_NAME);
    return 1;
  }
  gattcmd_module_enum_client(gattccmd_client_args.remote_addr->sval[0]);
  return 0;
}

static int gattccmd_write(int argc, char** argv) {
  int nerrros = arg_parse(argc, argv, (void**) &gattccmd_write_args);
  if (nerrros != 0) {
    arg_print_errors(stderr, gattccmd_write_args.end, GATTCMD_CMD_NAME);
    return 1;
  }
  gattcmd_module_gatt_write(gattccmd_write_args.addr->sval[0],
                            gattccmd_write_args.gatt->sval[0],
                            gattccmd_write_args.value->sval[0]);
  return 0;
}

static int gattccmd_scan(int argc, char** argv) {
  cat_console_register_ctrl_c_handler(&gattcmd_module_stop_workers);
  gattcmd_module_scan_client();
  return 0;
}

static int gattccmd_recon(int argc, char** argv) {
  cat_console_register_ctrl_c_handler(&gattcmd_module_stop_workers);
  gattcmd_module_recon();
  return 0;
}

void gattccmd_register_cmd() {
  gattccmd_client_args.remote_addr =
      arg_str0(NULL, NULL, "<BT Address>", "BT Address");
  gattccmd_client_args.end = arg_end(1);

  gattccmd_write_args.addr = arg_str0(NULL, NULL, "<BT Address>", "BT Address");
  gattccmd_write_args.gatt =
      arg_str1(NULL, NULL, "<GATT Address>", "GATT Address");
  gattccmd_write_args.value =
      arg_str1(NULL, NULL, "<Value>", "Value to write in hex form");
  gattccmd_write_args.end = arg_end(3);

  esp_console_cmd_t gattccmd_cmd_scan = {.command = "gattcmd_scan",
                                         .category = "BT",
                                         .hint = NULL,
                                         .help = "Scan for devices",
                                         .func = &gattccmd_scan,
                                         .argtable = NULL};

  esp_console_cmd_t gattccmd_set_client_cmd = {
      .command = "gattcmd_enum",
      .category = "BT",
      .hint = NULL,
      .help = "Enum the GATTs and Descriptors",
      .func = &gattccmd_enum_client,
      .argtable = &gattccmd_client_args};

  esp_console_cmd_t gattccmd_write_cmd = {
      .command = "gattcmd_write",
      .category = "BT",
      .hint = NULL,
      .func = &gattccmd_write,
      .help =
          "Write to the GATT, the command need a hex value for example: "
          "gattcmd_write 00:00:00:00:00:00 fff3 7e0404100001ff00ef",
      .argtable = &gattccmd_write_args};

  esp_console_cmd_t gattccmd_recon_cmd = {
      .command = "gattcmd_recon",
      .category = "BT",
      .hint = NULL,
      .func = &gattccmd_recon,
      .help = "Stop the BT scanning and connections services",
      .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&gattccmd_cmd_scan));
  ESP_ERROR_CHECK(esp_console_cmd_register(&gattccmd_set_client_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&gattccmd_write_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&gattccmd_recon_cmd));
}