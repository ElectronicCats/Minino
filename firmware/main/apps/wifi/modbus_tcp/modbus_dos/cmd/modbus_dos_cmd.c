#include "modbus_dos_cmd.h"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "modbus_dos_prefs.h"

#define TAG "Modbus Dos CMD"

static struct {
  struct arg_str* ssid;
  struct arg_end* end;
} mb_dos_ssid_args;

static struct {
  struct arg_str* pass;
  struct arg_end* end;
} mb_dos_pass_args;

static struct {
  struct arg_str* ip;
  struct arg_end* end;
} mb_dos_ip_args;

static struct {
  struct arg_int* port;
  struct arg_end* end;
} mb_dos_port_args;

static struct {
  struct arg_end* end;
} mb_dos_print_prefs_args;

static int set_ssid(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_dos_ssid_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_dos_ssid_args.end, "mb_dos_set_ssid");
    return 1;
  }

  modbus_dos_prefs_set_ssid(mb_dos_ssid_args.ssid->sval[0]);
  return 0;
}

static int set_pass(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_dos_pass_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_dos_pass_args.end, "mb_dos_set_pass");
    return 1;
  }

  modbus_dos_prefs_set_pass(mb_dos_pass_args.pass->sval[0]);
  return 0;
}

static int set_ip(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_dos_ip_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_dos_ip_args.end, "mb_dos_set_ip");
    return 1;
  }

  modbus_dos_prefs_set_ip(mb_dos_ip_args.ip->sval[0]);
  return 0;
}

static int set_port(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_dos_port_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_dos_port_args.end, "mb_dos_set_port");
    return 1;
  }
  int port = *mb_dos_port_args.port->ival;
  modbus_dos_prefs_set_port(port);
  return 0;
}

static int print_prefs(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mb_dos_print_prefs_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mb_dos_print_prefs_args.end, "mb_dos_print_prefs");
    return 1;
  }
  modbus_dos_prefs_print_prefs();
  return 0;
}

void modbus_dos_cmd_register_cmds() {
  mb_dos_ssid_args.ssid = arg_str0(NULL, NULL, "<ssid>", "Network SSID");
  mb_dos_ssid_args.end = arg_end(2);

  esp_console_cmd_t mb_dos_ssid_cmd = {
      .command = "mb_dos_set_ssid",
      .help = "\nSet Network SSID Example: mb_dos_set_ssid MyNetworkSSID",
      .category = "ModbusDOS",
      .hint = NULL,
      .func = &set_ssid,
      .argtable = &mb_dos_ssid_args};

  mb_dos_pass_args.pass =
      arg_str0(NULL, NULL, "<password>", "Network Password");
  mb_dos_pass_args.end = arg_end(2);

  esp_console_cmd_t mb_dos_pass_cmd = {
      .command = "mb_dos_set_pass",
      .help =
          "\nSet Network Password Example: mb_dos_set_pass MyNetworkPassword",
      .category = "ModbusDOS",
      .hint = NULL,
      .func = &set_pass,
      .argtable = &mb_dos_pass_args};

  mb_dos_ip_args.ip = arg_str0(NULL, NULL, "<ip>", "Server IP");
  mb_dos_ip_args.end = arg_end(2);

  esp_console_cmd_t mb_dos_ip_cmd = {
      .command = "mb_dos_set_ip",
      .help = "\nSet Server IP Example: mb_dos_set_ip 192.168.100.100",
      .category = "ModbusDOS",
      .hint = NULL,
      .func = &set_ip,
      .argtable = &mb_dos_ip_args};

  mb_dos_port_args.port = arg_int0(NULL, NULL, "<port>", "Server Port");
  mb_dos_port_args.end = arg_end(2);

  esp_console_cmd_t mb_dos_port_cmd = {
      .command = "mb_dos_set_port",
      .help = "\nSet Server Port Example: mb_dos_set_port 502",
      .category = "ModbusDOS",
      .hint = NULL,
      .func = &set_port,
      .argtable = &mb_dos_port_args};

  mb_dos_print_prefs_args.end = arg_end(2);

  esp_console_cmd_t mb_dos_print_prefs_cmd = {
      .command = "mb_dos_print_conf",
      .help = "\nPrint Modbus DOS Configuration",
      .category = "ModbusDOS",
      .hint = NULL,
      .func = &print_prefs,
      .argtable = &mb_dos_print_prefs_args};

  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_dos_ssid_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_dos_pass_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_dos_ip_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_dos_port_cmd));
  ESP_ERROR_CHECK(esp_console_cmd_register(&mb_dos_print_prefs_cmd));
}