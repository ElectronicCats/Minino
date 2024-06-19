#include "sd_card.h"

#include <string.h>
#include "argtable3/argtable3.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define MOUNT_POINT  "/sdcard"
#define PIN_NUM_MISO 20
#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  21
#define PIN_NUM_CS   18

static const char* TAG = "SD_CARD";
bool sd_card_mounted = false;

static struct {
  struct arg_str* device;
  struct arg_end* end;
} mount_args;

/** 'mount' command */
int mount(int argc, char** argv) {
  esp_err_t ret;

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
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config,
                                  &mount_config, &card);

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

void register_mount(void) {
  mount_args.device =
      arg_str1(NULL, NULL, "<sd>", "choose a proper device to mount/unmount");
  mount_args.end = arg_end(1);
}

int unmount(int argc, char** argv) {
  esp_err_t ret;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
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
    ret = spi_bus_free(host.slot);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to deinitialize bus.");
      return 1;
    }
  }
  return 0;
}

void register_unmount(void) {
  mount_args.device =
      arg_str1(NULL, NULL, "<sd>", "choose a proper device to mount/unmount");
  mount_args.end = arg_end(1);
}

void sd_card_init() {
  register_mount();
  register_unmount();
}

void sd_card_mount() {
  if (sd_card_mounted) {
    ESP_LOGW(TAG, "SD card already mounted");
    return;
  }

  const char** mount_argv[] = {"mount", "sd"};
  uint8_t mount_argc = 2;
  // TODO: return value form mount
  sd_card_mounted = true;
  mount(mount_argc, (char**) mount_argv);
}

void sd_card_unmount() {
  const char** unmount_argv2[] = {"unmount", "sd"};
  uint8_t unmount_argc2 = 2;
  sd_card_mounted = false;
  unmount(unmount_argc2, (char**) unmount_argv2);
}
