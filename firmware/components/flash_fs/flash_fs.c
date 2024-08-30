#include "flash_fs.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_spiffs.h"

#define TAG "Flash_fs"

size_t flash_fs_size, flash_fs_used;

esp_err_t flash_fs_mount() {
  esp_vfs_spiffs_conf_t conf = {.base_path = FLASH_FS_MOUNT_PATH,
                                .partition_label = "internal",
                                .max_files = 5,
                                .format_if_mount_failed = true};

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
    esp_spiffs_info(conf.partition_label, &flash_fs_size, &flash_fs_used);
    return ESP_OK;
  }
  return ret;
}
