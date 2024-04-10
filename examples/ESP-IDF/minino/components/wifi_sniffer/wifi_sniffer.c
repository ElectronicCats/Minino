/* Sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "linenoise/linenoise.h"
#include "sdkconfig.h"
// #if CONFIG_SNIFFER_PCAP_DESTINATION_SD
  #include "driver/sdmmc_host.h"
  #include "driver/sdspi_host.h"
  #include "driver/spi_common.h"
// #endif
#include "cmd_pcap.h"
#include "cmd_sniffer.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

#define MOUNT_POINT "/sdcard"
#define PIN_NUM_MISO 20
#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  21
#define PIN_NUM_CS   18

static const char* TAG = "WIFI_SNIFFER";

static struct {
  struct arg_str* device;
  struct arg_end* end;
} mount_args;

#if CONFIG_SNIFFER_STORE_HISTORY
#endif

static void initialize_nvs(void) {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

/* Initialize wifi with tcp/ip adapter */
static void initialize_wifi(void) {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

/** 'mount' command */
static int mount(int argc, char** argv) {
  esp_err_t ret;
  ESP_LOGW(TAG, "argc: %d", argc);
  // Print argv
  for (int i = 0; i < argc; i++) {
    ESP_LOGW(TAG, "argv[%d]: %s", i, argv[i]);
  }

  int nerrors = arg_parse(argc, argv, (void**) &mount_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mount_args.end, argv[0]);
    return 1;
  }
  /* mount sd card */
  if (!strncmp(mount_args.device->sval[0], "sd", 2)) {
    ESP_LOGI(TAG, "Initializing SD card");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024};

    // initialize SD card and mount FAT filesystem.
    sdmmc_card_t* card;

    ESP_LOGI(TAG, "Using SPI peripheral");
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize bus.");
      return 1;
    }

    // This initializes the slot without card detect (CD) and write protect (WP)
    // signals. Modify slot_config.gpio_cd and slot_config.gpio_wp if your board
    // has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    // ret = esp_vfs_fat_sdspi_mount(CONFIG_SNIFFER_MOUNT_POINT, &host,
    //                               &slot_config, &mount_config, &card);
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host,
                                  &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        ESP_LOGE(TAG,
                 "Failed to mount filesystem. "
                 "If you want the card to be formatted, set "
                 "format_if_mount_failed = true.");
      } else {
        ESP_LOGE(TAG,
                 "Failed to initialize the card (%s). "
                 "Make sure SD card lines have pull-up resistors in place.",
                 esp_err_to_name(ret));
      }
      return 1;
    }
    /* print card info if mount successfully */
    sdmmc_card_print_info(stdout, card);
  }
  return 0;
}

static void register_mount(void) {
  mount_args.device =
      arg_str1(NULL, NULL, "<sd>", "choose a proper device to mount/unmount");
  mount_args.end = arg_end(1);
}

static int unmount(int argc, char** argv) {
  int nerrors = arg_parse(argc, argv, (void**) &mount_args);
  if (nerrors != 0) {
    arg_print_errors(stderr, mount_args.end, argv[0]);
    return 1;
  }
  /* unmount sd card */
  if (!strncmp(mount_args.device->sval[0], "sd", 2)) {
    if (esp_vfs_fat_sdmmc_unmount() != ESP_OK) {
      ESP_LOGE(TAG, "Card unmount failed");
      return -1;
    }
    ESP_LOGI(TAG, "Card unmounted");
  }
  return 0;
}

static void register_unmount(void) {
  mount_args.device =
      arg_str1(NULL, NULL, "<sd>", "choose a proper device to mount/unmount");
  mount_args.end = arg_end(1);
}

void wifi_sniffer_init() {
  initialize_nvs();

  /*--- Initialize Network ---*/
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  /* Initialize WiFi */
  initialize_wifi();

  register_mount();
  register_unmount();
  register_sniffer_cmd();
  register_pcap_cmd();

  const char** mount_argv[] = {"mount", "sd"};
  uint8_t mount_argc = 2;
  mount(mount_argc, (char**) mount_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  const char** pcap_argv[] = {"pcap", "--open", "-f", "sniffer"};
  uint8_t pcap_argc = 4;
  do_pcap_cmd(pcap_argc, (char**) pcap_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  const char** sniffer_argv[] = {"sniffer", "-i", "wlan", "-c",
                                 "2",       "-n", "20"};
  uint8_t sniffer_argc = 7;
  do_sniffer_cmd(sniffer_argc, (char**) sniffer_argv);
  vTaskDelay(150000 / portTICK_PERIOD_MS);

  const char** stop_argv[] = {"sniffer", "--stop"};
  uint8_t stop_argc = 2;
  do_sniffer_cmd(stop_argc, (char**) stop_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  const char** close_argv[] = {"pcap", "--close", "-f", "sniffer"};
  uint8_t close_argc = 4;
  do_pcap_cmd(close_argc, (char**) close_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  const char** summary_argv[] = {"pcap", "--summary", "-f", "sniffer"};
  uint8_t summary_argc = 4;
  do_pcap_cmd(summary_argc, (char**) summary_argv);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  const char** unmount_argv2[] = {"unmount", "sd"};
  uint8_t unmount_argc2 = 2;
  unmount(unmount_argc2, (char**) unmount_argv2);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
