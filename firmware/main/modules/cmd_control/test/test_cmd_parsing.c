/* Host unit tests for console command parsing and input validation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "unity.h"

#include "cat_console.h"
#include "catdos_module.h"
#include "cmd_catdos.h"
#include "cmd_control.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_flash_storage.h"
#include "menus_module.h"
#include "uart_bridge.h"

/* =========================================================================
 * Mock state and implementations
 * ========================================================================= */

#define MAX_RECORDED_COMMANDS 64

typedef struct {
  char name[32];
  esp_console_cmd_func_t func;
  void* argtable;
} recorded_cmd_t;

static recorded_cmd_t s_commands[MAX_RECORDED_COMMANDS];
static int s_command_count = 0;

static char s_last_launched_app[64] = {0};
static bool s_task_created = false;
static esp_reset_reason_t s_mock_reset_reason = ESP_RST_POWERON;

static char s_uart_written_data[256] = {0};
static size_t s_uart_written_len = 0;
static uart_bridge_config_t s_uart_config = {0};

static char s_target_host[64] = {0};
static char s_target_port[64] = {0};
static char s_target_endpoint[64] = {0};

static char s_saved_spam_name[32] = {0};
static char s_saved_spam_val[128] = {0};
static char s_deleted_spam_item[32] = {0};

int esp_console_cmd_register(const esp_console_cmd_t* cmd) {
  if (s_command_count < MAX_RECORDED_COMMANDS) {
    strncpy(s_commands[s_command_count].name, cmd->command,
            sizeof(s_commands[s_command_count].name) - 1);
    s_commands[s_command_count].func = cmd->func;
    s_commands[s_command_count].argtable = cmd->argtable;
    s_command_count++;
  }
  return 0;
}

static esp_console_cmd_func_t find_command(const char* name) {
  for (int i = 0; i < s_command_count; i++) {
    if (strcmp(s_commands[i].name, name) == 0) {
      return s_commands[i].func;
    }
  }
  return NULL;
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char* const pcName,
                       const uint32_t usStackDepth,
                       void* const pvParameters,
                       uint32_t uxPriority,
                       TaskHandle_t* const pxCreatedTask) {
  (void) usStackDepth;
  (void) uxPriority;
  (void) pxCreatedTask;
  s_task_created = true;
  if (strcmp(pcName, "launch_app_task") == 0 && pvParameters != NULL) {
    strncpy(s_last_launched_app, (const char*) pvParameters,
            sizeof(s_last_launched_app) - 1);
  }
  (void) pxTaskCode;
  return pdPASS;
}

void vTaskDelete(TaskHandle_t xTaskToDelete) {
  (void) xTaskToDelete;
}

void vTaskDelay(uint32_t xTicksToDelay) {
  (void) xTicksToDelay;
}

void menus_module_set_menu_over_cmd(char* cmd) {
  if (cmd) {
    strncpy(s_last_launched_app, cmd, sizeof(s_last_launched_app) - 1);
  }
}

bool menus_module_get_app_state(void) {
  return false;
}

void menus_module_exit_app(void) {}

void menus_module_refresh(void) {}

menu_idx_t menus_module_get_current_menu(void) {
  return 0;
}

bool keyboard_module_inject_event(uint8_t button, uint8_t event) {
  (void) button;
  (void) event;
  return true;
}

size_t oled_screen_ink(void) {
  return 100;
}

void esp_sdr_module_begin(void) {}
void esp_sdr_module_stop(void) {}

esp_reset_reason_t esp_reset_reason(void) {
  return s_mock_reset_reason;
}

void esp_restart(void) {}

void esp_chip_info(esp_chip_info_t* out_info) {
  if (out_info) {
    out_info->features = CHIP_FEATURE_WIFI_BGN | CHIP_FEATURE_BLE;
    out_info->cores = 1;
    out_info->revision = 1;
  }
}

esp_err_t esp_flash_get_size(void* chip, uint32_t* out_size) {
  (void) chip;
  if (out_size) {
    *out_size = 4 * 1024 * 1024;
  }
  return ESP_OK;
}

uint32_t heap_caps_get_free_size(uint32_t caps) {
  (void) caps;
  return 128 * 1024;
}

uint32_t heap_caps_get_minimum_free_size(uint32_t caps) {
  (void) caps;
  return 64 * 1024;
}

uint32_t esp_get_free_heap_size(void) {
  return 128 * 1024;
}

void cat_console_register_ctrl_c_handler(ctrl_c_callback_t callback) {
  (void) callback;
}

esp_err_t uart_bridge_begin(uart_config_t uart_config, int buffer_size) {
  s_uart_config.uart_config = uart_config;
  s_uart_config.buffer_size = buffer_size;
  return ESP_OK;
}

uart_bridge_config_t uart_bridge_get_config(void) {
  return s_uart_config;
}

int uart_bridge_write(const char* src, size_t size) {
  if (src != NULL && size > 0) {
    size_t to_copy = size < sizeof(s_uart_written_data) - 1
                         ? size
                         : sizeof(s_uart_written_data) - 1;
    memcpy(s_uart_written_data, src, to_copy);
    s_uart_written_data[to_copy] = '\0';
    s_uart_written_len = to_copy;
  }
  return size;
}

esp_err_t uart_bridge_read(char* buf, int length, int timeout_ms) {
  (void) buf;
  (void) length;
  (void) timeout_ms;
  return ESP_OK;
}

void catdos_module_set_target(char* host, char* port, char* endpoint) {
  if (host)
    strncpy(s_target_host, host, sizeof(s_target_host) - 1);
  if (port)
    strncpy(s_target_port, port, sizeof(s_target_port) - 1);
  if (endpoint)
    strncpy(s_target_endpoint, endpoint, sizeof(s_target_endpoint) - 1);
}

int catdos_module_send_attack(int argc, char** argv) {
  (void) argc;
  (void) argv;
  return 0;
}

uint8_t flash_storage_get_list(char* main_name,
                               storage_contex_t* list,
                               uint8_t capacity) {
  (void) main_name;
  static char mock_item1[] = "ssid1";
  static char mock_item2[] = "ssid2";
  if (list && capacity >= 2) {
    memset(list, 0, capacity * sizeof(storage_contex_t));
    list[0].item_storage_name = mock_item1;
    list[1].item_storage_name = mock_item2;
  }
  return 2;
}

void flash_storage_show_list(char* main_name) {
  (void) main_name;
}

void flash_storage_save_list_items(storage_contex_t* storage_context) {
  if (storage_context) {
    if (storage_context->item_storage_name) {
      strncpy(s_saved_spam_name, storage_context->item_storage_name,
              sizeof(s_saved_spam_name) - 1);
    }
    if (storage_context->items_storage_value) {
      strncpy(s_saved_spam_val, storage_context->items_storage_value,
              sizeof(s_saved_spam_val) - 1);
    }
  }
}

void flash_storage_delete_list_item(char* main_tree, char* subitem) {
  (void) main_tree;
  if (subitem) {
    strncpy(s_deleted_spam_item, subitem, sizeof(s_deleted_spam_item) - 1);
  }
}

/* External command registrations */
extern void cmd_control_register_launch_cmd(void);
extern void cmd_control_register_sdr_cmd(void);
extern void cmd_control_register_system_commands(void);
extern void cmd_control_register_uart_bridge_commands(void);
extern void register_catdos_commands(void);
extern void cmd_spamssid_register_commands(void);

/* The command control hands the USB port over to the SDR host mode. The
   test does not open any port, so the call goes nowhere. */
esp_err_t esdr_host_run(void) {
  return 0;
}

static void register_all_commands_once(void) {
  static bool registered = false;
  if (!registered) {
    cmd_control_register_launch_cmd();
    cmd_control_register_sdr_cmd();
    cmd_control_register_system_commands();
    cmd_control_register_uart_bridge_commands();
    register_catdos_commands();
    cmd_spamssid_register_commands();
    cmd_control_register_threshold_cmd();
    registered = true;
  }
}

void setUp(void) {
  register_all_commands_once();
  s_task_created = false;
  s_last_launched_app[0] = '\0';
  s_uart_written_data[0] = '\0';
  s_uart_written_len = 0;
  s_target_host[0] = '\0';
  s_target_port[0] = '\0';
  s_target_endpoint[0] = '\0';
  s_saved_spam_name[0] = '\0';
  s_saved_spam_val[0] = '\0';
  s_deleted_spam_item[0] = '\0';
  s_mock_reset_reason = ESP_RST_POWERON;
}

void tearDown(void) {}

/* =========================================================================
 * 1. Launch Command Tests (argc/argv validation & NULL pointer safety)
 * ========================================================================= */

void test_launch_app_no_arguments_fails_gracefully(void) {
  esp_console_cmd_func_t launch_fn = find_command("launch");
  TEST_ASSERT_NOT_NULL(launch_fn);

  /* User just types "launch" with argc=1: should NOT crash or deref NULL */
  char* argv[] = {"launch", NULL};
  int ret = launch_fn(1, argv);
  TEST_ASSERT_EQUAL_INT(1, ret);
  TEST_ASSERT_FALSE(s_task_created);
}

void test_launch_app_null_argv_fails_gracefully(void) {
  esp_console_cmd_func_t launch_fn = find_command("launch");
  TEST_ASSERT_NOT_NULL(launch_fn);

  /* argc=0 or argv=NULL */
  int ret = launch_fn(0, NULL);
  TEST_ASSERT_EQUAL_INT(1, ret);

  char* argv_null_cmd[] = {NULL};
  ret = launch_fn(1, argv_null_cmd);
  TEST_ASSERT_EQUAL_INT(1, ret);
  TEST_ASSERT_FALSE(s_task_created);
}

void test_launch_app_valid_argument(void) {
  esp_console_cmd_func_t launch_fn = find_command("launch");
  TEST_ASSERT_NOT_NULL(launch_fn);

  char* argv[] = {"launch", "analyzer", NULL};
  int ret = launch_fn(2, argv);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_TRUE(s_task_created);
  TEST_ASSERT_EQUAL_STRING("analyzer", s_last_launched_app);
}

void test_sdr_capture_argc_validation(void) {
  esp_console_cmd_func_t sdr_fn = find_command("sdr_capture");
  TEST_ASSERT_NOT_NULL(sdr_fn);

  /* Unexpected argument to sdr_capture must fail */
  char* argv_bad[] = {"sdr_capture", "unexpected", NULL};
  int ret = sdr_fn(2, argv_bad);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Valid invocation with argc=1 */
  char* argv_good[] = {"sdr_capture", NULL};
  ret = sdr_fn(1, argv_good);
  TEST_ASSERT_EQUAL_INT(0, ret);
}

/* =========================================================================
 * 2. UART Bridge Command Tests (Bug fix: println vs print, parameter bounds)
 * ========================================================================= */

void test_uart_bridge_println_parses_its_own_arguments(void) {
  esp_console_cmd_func_t println_fn = find_command("println");
  TEST_ASSERT_NOT_NULL(println_fn);

  /* Missing message argument: returns 1 without asserting/crashing */
  char* argv_no_args[] = {"println", NULL};
  int ret = println_fn(1, argv_no_args);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Valid message: must correctly parse println_args */
  char* argv_good[] = {"println", "test_message", NULL};
  ret = println_fn(2, argv_good);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_EQUAL_STRING("\n", s_uart_written_data);
}

void test_uart_bridge_print_missing_argument_handled(void) {
  esp_console_cmd_func_t print_fn = find_command("print");
  TEST_ASSERT_NOT_NULL(print_fn);

  char* argv_no_args[] = {"print", NULL};
  int ret = print_fn(1, argv_no_args);
  TEST_ASSERT_EQUAL_INT(1, ret);

  char* argv_good[] = {"print", "hello_uart", NULL};
  ret = print_fn(2, argv_good);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_EQUAL_STRING("hello_uart", s_uart_written_data);
}

void test_uart_bridge_set_buffer_size_bounds(void) {
  esp_console_cmd_func_t set_buf_fn =
      find_command("uart_bridge_set_buffer_size");
  TEST_ASSERT_NOT_NULL(set_buf_fn);

  /* Negative buffer size rejected */
  char* argv_neg[] = {"uart_bridge_set_buffer_size", "-10", NULL};
  int ret = set_buf_fn(2, argv_neg);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* 0 buffer size rejected */
  char* argv_zero[] = {"uart_bridge_set_buffer_size", "0", NULL};
  ret = set_buf_fn(2, argv_zero);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Oversized buffer (> 4096) rejected */
  char* argv_huge[] = {"uart_bridge_set_buffer_size", "100000", NULL};
  ret = set_buf_fn(2, argv_huge);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Valid buffer size accepted */
  char* argv_valid[] = {"uart_bridge_set_buffer_size", "2048", NULL};
  ret = set_buf_fn(2, argv_valid);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_EQUAL_INT(2048, s_uart_config.buffer_size);
}

void test_uart_bridge_set_baud_rate_validation(void) {
  esp_console_cmd_func_t set_baud_fn =
      find_command("uart_bridge_set_baud_rate");
  TEST_ASSERT_NOT_NULL(set_baud_fn);

  /* Negative / zero baud rejected */
  char* argv_zero[] = {"uart_bridge_set_baud_rate", "0", NULL};
  TEST_ASSERT_EQUAL_INT(1, set_baud_fn(2, argv_zero));

  char* argv_neg[] = {"uart_bridge_set_baud_rate", "-9600", NULL};
  TEST_ASSERT_EQUAL_INT(1, set_baud_fn(2, argv_neg));

  /* Valid baud accepted */
  char* argv_valid[] = {"uart_bridge_set_baud_rate", "115200", NULL};
  TEST_ASSERT_EQUAL_INT(0, set_baud_fn(2, argv_valid));
  TEST_ASSERT_EQUAL_INT(115200, s_uart_config.uart_config.baud_rate);
}

void test_uart_bridge_set_config_bounds(void) {
  esp_console_cmd_func_t set_cfg_fn = find_command("uart_bridge_set_config");
  TEST_ASSERT_NOT_NULL(set_cfg_fn);

  /* Missing args rejected */
  char* argv_missing[] = {"uart_bridge_set_config", "1024", "115200", NULL};
  TEST_ASSERT_EQUAL_INT(1, set_cfg_fn(3, argv_missing));

  /* Invalid parity (must be 0, 2, or 3) */
  char* argv_bad_parity[] = {
      "uart_bridge_set_config", "1024", "115200", "3", "4", "1", "0", NULL};
  TEST_ASSERT_EQUAL_INT(1, set_cfg_fn(7, argv_bad_parity));

  /* Valid configuration */
  char* argv_valid[] = {
      "uart_bridge_set_config", "1024", "115200", "3", "0", "1", "0", NULL};
  TEST_ASSERT_EQUAL_INT(0, set_cfg_fn(7, argv_valid));
  TEST_ASSERT_EQUAL_INT(1024, s_uart_config.buffer_size);
  TEST_ASSERT_EQUAL_INT(115200, s_uart_config.uart_config.baud_rate);
}

/* =========================================================================
 * 3. CatDOS Web Config Tests (String overflow prevention & argument checks)
 * ========================================================================= */

void test_catdos_web_config_missing_args(void) {
  esp_console_cmd_func_t web_cfg_fn = find_command("web_config");
  TEST_ASSERT_NOT_NULL(web_cfg_fn);

  /* Missing endpoint */
  char* argv_missing[] = {"web_config", "192.168.1.1", "80", NULL};
  int ret = web_cfg_fn(3, argv_missing);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* No args */
  char* argv_none[] = {"web_config", NULL};
  ret = web_cfg_fn(1, argv_none);
  TEST_ASSERT_EQUAL_INT(1, ret);
}

void test_catdos_web_config_string_overflow_prevented(void) {
  esp_console_cmd_func_t web_cfg_fn = find_command("web_config");
  TEST_ASSERT_NOT_NULL(web_cfg_fn);

  /* String of 32 characters or more: must be rejected to prevent buffer
   * overflow */
  char long_host[64];
  memset(long_host, 'A', 35);
  long_host[35] = '\0';

  char* argv_overflow[] = {"web_config", long_host, "80", "/", NULL};
  int ret = web_cfg_fn(4, argv_overflow);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Port string of 32 characters rejected */
  char* argv_overflow_port[] = {"web_config", "10.0.0.1", long_host, "/", NULL};
  ret = web_cfg_fn(4, argv_overflow_port);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Valid configuration under 32 bytes accepted */
  char* argv_valid[] = {"web_config", "192.168.4.1", "8080", "/api/test", NULL};
  ret = web_cfg_fn(4, argv_valid);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_EQUAL_STRING("192.168.4.1", s_target_host);
  TEST_ASSERT_EQUAL_STRING("8080", s_target_port);
  TEST_ASSERT_EQUAL_STRING("/api/test", s_target_endpoint);
}

/* =========================================================================
 * 4. Spam SSID Command Tests (String limits, numeric validation, memory leak)
 * ========================================================================= */

void test_spam_save_ssid_length_limit(void) {
  esp_console_cmd_func_t save_fn = find_command("spam_save");
  TEST_ASSERT_NOT_NULL(save_fn);

  /* SSID longer than 17 chars rejected */
  char* argv_long[] = {"spam_save", "--name=VeryLongSSIDNameExceedingLimit",
                       "word1,word2", NULL};
  int ret = save_fn(3, argv_long);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Valid SSID name and value */
  char* argv_valid[] = {"spam_save", "--name=MySSID", "wifi1,wifi2", NULL};
  ret = save_fn(3, argv_valid);
  TEST_ASSERT_EQUAL_INT(0, ret);
  TEST_ASSERT_EQUAL_STRING("MySSID", s_saved_spam_name);
}

void test_spam_delete_numeric_index_validation(void) {
  esp_console_cmd_func_t del_fn = find_command("spam_delete");
  TEST_ASSERT_NOT_NULL(del_fn);

  /* Non-numeric index rejected */
  char* argv_non_num[] = {"spam_delete", "not_a_number", NULL};
  int ret = del_fn(2, argv_non_num);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Out of range index rejected (list_count mocked to 2) */
  char* argv_oob[] = {"spam_delete", "99", NULL};
  ret = del_fn(2, argv_oob);
  TEST_ASSERT_EQUAL_INT(1, ret);

  /* Negative index rejected */
  char* argv_neg[] = {"spam_delete", "-1", NULL};
  ret = del_fn(2, argv_neg);
  TEST_ASSERT_EQUAL_INT(1, ret);
}

/* =========================================================================
 * 5. System Reset Reason Tests (NULL pointer prevention for unknown codes)
 * ========================================================================= */

void test_print_reset_reason_unknown_code_no_null_dereference(void) {
  extern int print_reset_reason(int argc, char** argv);

  /* Test with standard reason */
  s_mock_reset_reason = ESP_RST_POWERON;
  char* argv[] = {"get_reset_reason", NULL};
  int ret = print_reset_reason(1, argv);
  TEST_ASSERT_EQUAL_INT(0, ret);

  /* Test with an unexpected enum value (e.g. 999) that wasn't in the switch.
   * Must NOT dereference NULL in printf! */
  s_mock_reset_reason = (esp_reset_reason_t) 999;
  ret = print_reset_reason(1, argv);
  TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_threshold_command_parsing(void) {
  esp_console_cmd_func_t th_fn = find_command("threshold");
  TEST_ASSERT_NOT_NULL(th_fn);

  /* No args -> prints status (ret 0) */
  char* argv_status[] = {"threshold", NULL};
  TEST_ASSERT_EQUAL_INT(0, th_fn(1, argv_status));

  /* "off" -> ret 0 */
  char* argv_off[] = {"threshold", "off", NULL};
  TEST_ASSERT_EQUAL_INT(0, th_fn(2, argv_off));

  /* Valid dBm -> ret 0 */
  char* argv_valid[] = {"threshold", "-75", NULL};
  TEST_ASSERT_EQUAL_INT(0, th_fn(2, argv_valid));

  /* Auto-negate positive number -> ret 0 */
  char* argv_auto_neg[] = {"threshold", "75", NULL};
  TEST_ASSERT_EQUAL_INT(0, th_fn(2, argv_auto_neg));

  /* Out of bounds -> ret 1 */
  char* argv_oob[] = {"threshold", "-120", NULL};
  TEST_ASSERT_EQUAL_INT(1, th_fn(2, argv_oob));
}

void test_spectrum_advanced_commands_parsing(void) {
  /* spectrum_audio */
  esp_console_cmd_func_t aud_fn = find_command("spectrum_audio");
  TEST_ASSERT_NOT_NULL(aud_fn);
  char* argv_aud_stat[] = {"spectrum_audio", NULL};
  TEST_ASSERT_EQUAL_INT(0, aud_fn(1, argv_aud_stat));
  char* argv_aud_on[] = {"spectrum_audio", "on", NULL};
  TEST_ASSERT_EQUAL_INT(0, aud_fn(2, argv_aud_on));
  char* argv_aud_off[] = {"spectrum_audio", "off", NULL};
  TEST_ASSERT_EQUAL_INT(0, aud_fn(2, argv_aud_off));
  char* argv_aud_bad[] = {"spectrum_audio", "invalid", NULL};
  TEST_ASSERT_EQUAL_INT(1, aud_fn(2, argv_aud_bad));

  /* spectrum_log */
  esp_console_cmd_func_t log_fn = find_command("spectrum_log");
  TEST_ASSERT_NOT_NULL(log_fn);
  char* argv_log_stat[] = {"spectrum_log", NULL};
  TEST_ASSERT_EQUAL_INT(0, log_fn(1, argv_log_stat));
  char* argv_log_on[] = {"spectrum_log", "on", NULL};
  TEST_ASSERT_EQUAL_INT(0, log_fn(2, argv_log_on));
  char* argv_log_off[] = {"spectrum_log", "off", NULL};
  TEST_ASSERT_EQUAL_INT(0, log_fn(2, argv_log_off));

  /* spectrum_band */
  esp_console_cmd_func_t band_fn = find_command("spectrum_band");
  TEST_ASSERT_NOT_NULL(band_fn);
  char* argv_band_stat[] = {"spectrum_band", NULL};
  TEST_ASSERT_EQUAL_INT(0, band_fn(1, argv_band_stat));
  char* argv_band_wifi[] = {"spectrum_band", "wifi", NULL};
  TEST_ASSERT_EQUAL_INT(0, band_fn(2, argv_band_wifi));
  char* argv_band_zb[] = {"spectrum_band", "zigbee", NULL};
  TEST_ASSERT_EQUAL_INT(0, band_fn(2, argv_band_zb));
  char* argv_band_bad[] = {"spectrum_band", "bluetooth", NULL};
  TEST_ASSERT_EQUAL_INT(1, band_fn(2, argv_band_bad));

  /* spectrum_ids */
  esp_console_cmd_func_t ids_fn = find_command("spectrum_ids");
  TEST_ASSERT_NOT_NULL(ids_fn);
  char* argv_ids_stat[] = {"spectrum_ids", NULL};
  TEST_ASSERT_EQUAL_INT(0, ids_fn(1, argv_ids_stat));
  char* argv_ids_clr[] = {"spectrum_ids", "clear", NULL};
  TEST_ASSERT_EQUAL_INT(0, ids_fn(2, argv_ids_clr));

  /* spectrum_pcap */
  esp_console_cmd_func_t pcap_fn = find_command("spectrum_pcap");
  TEST_ASSERT_NOT_NULL(pcap_fn);
  char* argv_pcap_stat[] = {"spectrum_pcap", NULL};
  TEST_ASSERT_EQUAL_INT(0, pcap_fn(1, argv_pcap_stat));
  char* argv_pcap_on[] = {"spectrum_pcap", "on", NULL};
  TEST_ASSERT_EQUAL_INT(0, pcap_fn(2, argv_pcap_on));
  char* argv_pcap_off[] = {"spectrum_pcap", "off", NULL};
  TEST_ASSERT_EQUAL_INT(0, pcap_fn(2, argv_pcap_off));
  char* argv_pcap_bad[] = {"spectrum_pcap", "unknown", NULL};
  TEST_ASSERT_EQUAL_INT(1, pcap_fn(2, argv_pcap_bad));

  /* spectrum_drone */
  esp_console_cmd_func_t drone_fn = find_command("spectrum_drone");
  TEST_ASSERT_NOT_NULL(drone_fn);
  char* argv_drone_stat[] = {"spectrum_drone", NULL};
  TEST_ASSERT_EQUAL_INT(0, drone_fn(1, argv_drone_stat));
  char* argv_drone_clr[] = {"spectrum_drone", "clear", NULL};
  TEST_ASSERT_EQUAL_INT(0, drone_fn(2, argv_drone_clr));
}

int main(void) {
  UNITY_BEGIN();

  /* 1. Launch & sdr_capture */
  RUN_TEST(test_launch_app_no_arguments_fails_gracefully);
  RUN_TEST(test_launch_app_null_argv_fails_gracefully);
  RUN_TEST(test_launch_app_valid_argument);
  RUN_TEST(test_sdr_capture_argc_validation);

  /* 2. UART bridge */
  RUN_TEST(test_uart_bridge_println_parses_its_own_arguments);
  RUN_TEST(test_uart_bridge_print_missing_argument_handled);
  RUN_TEST(test_uart_bridge_set_buffer_size_bounds);
  RUN_TEST(test_uart_bridge_set_baud_rate_validation);
  RUN_TEST(test_uart_bridge_set_config_bounds);

  /* 3. CatDOS Web Config */
  RUN_TEST(test_catdos_web_config_missing_args);
  RUN_TEST(test_catdos_web_config_string_overflow_prevented);

  /* 4. Spam SSID */
  RUN_TEST(test_spam_save_ssid_length_limit);
  RUN_TEST(test_spam_delete_numeric_index_validation);

  /* 5. System Commands */
  RUN_TEST(test_print_reset_reason_unknown_code_no_null_dereference);

  /* 6. RF Threshold Command */
  RUN_TEST(test_threshold_command_parsing);
  RUN_TEST(test_spectrum_advanced_commands_parsing);

  return UNITY_END();
}
