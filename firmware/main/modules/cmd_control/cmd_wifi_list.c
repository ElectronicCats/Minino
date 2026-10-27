#include "argtable3/argtable3.h"
#include "cmd_wifi.h"
#include "esp_console.h"
#include "wifi_scanner.h"

static wifi_scanner_ap_records_t* ap_records;

static void cmd_wifi_run_scan_task(void* pvParameters) {
  (void) pvParameters;
  uint8_t scan_count = 0;
  ap_records = wifi_scanner_get_ap_records();
  if (ap_records == NULL) {
    vTaskDelete(NULL);
    return;
  }
  while (ap_records != NULL &&
         ap_records->count < (DEFAULT_SCAN_LIST_SIZE / 2) && scan_count < 3) {
    wifi_scanner_module_scan();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    scan_count++;
    ap_records = wifi_scanner_get_ap_records();
  }

  ap_records = wifi_scanner_get_ap_records();
  if (ap_records != NULL) {
    wifi_scanner_show_records_ssid();
  }
  vTaskDelete(NULL);
}

static int cmd_wifi_scan(int argc, char** argv) {
  (void) argc;
  (void) argv;
  BaseType_t ret =
      xTaskCreate(cmd_wifi_run_scan_task, "wifi_scan", 8096, NULL, 5, NULL);
  if (ret != pdPASS) {
    return 1;
  }
  return 0;
}

void cmd_wifi_list_register_commands() {
  esp_console_cmd_t wifi_scanner_cmd = {
      .command = "wifi_scan",
      .help = "Scan for SSID",
      .category = NULL,
      .hint = NULL,
      .func = &cmd_wifi_scan,
      .argtable = NULL,
  };

  ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_scanner_cmd));
}