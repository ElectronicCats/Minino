#include "flash_fs.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_spiffs.h"

#define TAG "Flash_fs"

static size_t flash_fs_size, flash_fs_used;

static void (*flash_fs_show_event_cb)(flash_fs_events_t, void*) = NULL;
static bool already_mounted = false;

void flash_fs_begin(void* screens_cb) {
  flash_fs_show_event_cb = screens_cb;
}

static void flash_fs_show_event(flash_fs_events_t event, void* ctx) {
  if (flash_fs_show_event_cb) {
    flash_fs_show_event_cb(event, ctx);
  }
}

esp_err_t flash_fs_mount() {
  esp_vfs_spiffs_conf_t conf = {.base_path = FLASH_FS_MOUNT_PATH,
                                .partition_label = "internal",
                                .max_files = 5,
                                .format_if_mount_failed = true};

  if (!esp_spiffs_mounted(conf.partition_label)) {
    flash_fs_show_event(FLASH_FS_MOUNTING_EV, NULL);
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
      esp_spiffs_info(conf.partition_label, &flash_fs_size, &flash_fs_used);
      flash_fs_show_event(FLASH_FS_RESULT_EV, ESP_OK);
      return ESP_OK;
    }
    flash_fs_show_event(FLASH_FS_RESULT_EV, ret);
    return ret;
  }
  return ESP_OK;
}
