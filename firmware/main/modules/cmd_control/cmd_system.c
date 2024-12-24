#include "cmd_control.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_system.h"

static char* category = "System";

uint8_t print_free_heap() {
  printf("Free heap size: %d bytes or %d KB\n",
         heap_caps_get_free_size(MALLOC_CAP_8BIT),
         heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);
  return 0;
}

uint8_t print_min_free_heap() {
  printf("Minimum free heap size: %d bytes or %d KB\n",
         heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
         heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT) / 1024);
  return 0;
}

uint8_t print_chip_info() {
  esp_chip_info_t chip_info;
  uint32_t flash_size;
  esp_chip_info(&chip_info);
  printf("Chip info:\n");
  printf("\tModel: %s\n", CONFIG_IDF_TARGET);
  printf("\tCores: %d\n", chip_info.cores);
  printf("\tFeatures: ");
  if (chip_info.features & CHIP_FEATURE_WIFI_BGN) {
    printf("WiFi/");
  }
  if (chip_info.features & CHIP_FEATURE_BT) {
    printf("BT");
  }
  if (chip_info.features & CHIP_FEATURE_BLE) {
    printf("BLE");
  }
  if (chip_info.features & CHIP_FEATURE_IEEE802154) {
    printf(", 802.15.4 (Zigbee/Thread)");
  }
  printf("\n");
  unsigned major_rev = chip_info.revision / 100;
  unsigned minor_rev = chip_info.revision % 100;
  printf("\tSilicon revision: v%d.%d\n", major_rev, minor_rev);
  if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
    printf("Get flash size failed");
    return 1;
  }
  printf(
      "\tFlash size: %" PRIu32 "MB %s flash\n",
      flash_size / (uint32_t) (1024 * 1024),
      (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  return 0;
}

uint8_t print_reset_reason() {
  const char* reset_reason = NULL;
  switch (esp_reset_reason()) {
    case ESP_RST_UNKNOWN:
      reset_reason = "Unknown reset";
      break;
    case ESP_RST_POWERON:
      reset_reason = "Power-on reset";
      break;
    case ESP_RST_EXT:
      reset_reason = "External reset";
      break;
    case ESP_RST_SW:
      reset_reason = "Software reset";
      break;
    case ESP_RST_PANIC:
      reset_reason = "Exception reset (panic)";
      break;
    case ESP_RST_INT_WDT:
      reset_reason = "Watchdog reset";
      break;
    case ESP_RST_TASK_WDT:
      reset_reason = "Task watchdog reset";
      break;
    case ESP_RST_WDT:
      reset_reason = "Other watchdog reset";
      break;
    case ESP_RST_DEEPSLEEP:
      reset_reason = "Deep sleep reset";
      break;
    case ESP_RST_BROWNOUT:
      reset_reason = "Brownout reset";
      break;
    case ESP_RST_SDIO:
      reset_reason = "SDIO reset";
      break;
    case ESP_RST_USB:
      reset_reason = "USB reset";
      break;
    case ESP_RST_JTAG:
      reset_reason = "JTAG reset";
      break;
    case ESP_RST_EFUSE:
      reset_reason = "Efuse reset";
      break;
    case ESP_RST_PWR_GLITCH:
      reset_reason = "Power glitch reset";
      break;
    case ESP_RST_CPU_LOCKUP:
      reset_reason = "CPU lockup reset";
      break;
  }
  printf("Reset reason: %s\n", reset_reason);
  return 0;
}

void cmd_control_register_system_commands() {
  esp_console_cmd_t restart = {.command = "reset",
                               .help = "Restart the device",
                               .hint = NULL,
                               .category = category,
                               .func = &esp_restart,
                               .argtable = NULL};

  esp_console_cmd_t get_free_heap = {.command = "get_free_heap",
                                     .help = "Get the free heap size",
                                     .hint = NULL,
                                     .category = category,
                                     .func = &print_free_heap,
                                     .argtable = NULL};

  esp_console_cmd_t get_min_free_heap = {
      .command = "get_min_free_heap",
      .help = "Get the minimum free heap size",
      .hint = NULL,
      .func = &print_min_free_heap,
      .argtable = NULL};

  esp_console_cmd_t get_chip_info = {.command = "get_chip_info",
                                     .help = "Get the chip information",
                                     .hint = NULL,
                                     .category = category,
                                     .func = &print_chip_info,
                                     .argtable = NULL};

  esp_console_cmd_t get_reset_reason = {.command = "get_reset_reason",
                                        .help = "Get the reset reason",
                                        .hint = NULL,
                                        .category = category,
                                        .func = &print_reset_reason,
                                        .argtable = NULL};

  ESP_ERROR_CHECK(esp_console_cmd_register(&restart));
  ESP_ERROR_CHECK(esp_console_cmd_register(&get_free_heap));
  ESP_ERROR_CHECK(esp_console_cmd_register(&get_min_free_heap));
  ESP_ERROR_CHECK(esp_console_cmd_register(&get_chip_info));
  ESP_ERROR_CHECK(esp_console_cmd_register(&get_reset_reason));
}
