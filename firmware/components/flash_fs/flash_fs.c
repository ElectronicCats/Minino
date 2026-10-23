#include "flash_fs.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "Flash_fs"

static size_t flash_fs_size = 0;
static size_t flash_fs_used = 0;
static SemaphoreHandle_t flash_fs_mutex = NULL;

static void (*flash_fs_show_event_cb)(flash_fs_events_t, void*) = NULL;

/* Must be RECURSIVE: flash_fs_mount(), flash_fs_unmount() and
   flash_fs_get_info() all take this mutex and then call flash_fs_show_event(),
   which takes it again. With a plain xSemaphoreCreateMutex() the inner take
   blocks the outer holder on itself for portMAX_DELAY, so the first
   flash_fs_mount() hung the calling task forever - taking the File Manager and
   the web file browser down with it. xQueueSemaphoreTake() has no holder
   check, so a non-recursive mutex cannot express this nesting. */
static void flash_fs_init_mutex_if_needed(void) {
  if (flash_fs_mutex == NULL) {
    flash_fs_mutex = xSemaphoreCreateRecursiveMutex();
  }
}

void flash_fs_begin(void* screens_cb) {
  flash_fs_init_mutex_if_needed();
  if (flash_fs_mutex &&
      xSemaphoreTakeRecursive(flash_fs_mutex, portMAX_DELAY) == pdTRUE) {
    flash_fs_show_event_cb = screens_cb;
    xSemaphoreGiveRecursive(flash_fs_mutex);
  } else {
    flash_fs_show_event_cb = screens_cb;
  }
}

static void flash_fs_show_event(flash_fs_events_t event, void* ctx) {
  void (*cb)(flash_fs_events_t, void*) = NULL;
  flash_fs_init_mutex_if_needed();
  if (flash_fs_mutex &&
      xSemaphoreTakeRecursive(flash_fs_mutex, portMAX_DELAY) == pdTRUE) {
    cb = flash_fs_show_event_cb;
    xSemaphoreGiveRecursive(flash_fs_mutex);
  } else {
    cb = flash_fs_show_event_cb;
  }

  if (cb) {
    cb(event, ctx);
  }
}

bool flash_fs_is_mounted(void) {
  return esp_spiffs_mounted("internal");
}

esp_err_t flash_fs_mount(void) {
  flash_fs_init_mutex_if_needed();
  if (flash_fs_mutex &&
      xSemaphoreTakeRecursive(flash_fs_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  esp_vfs_spiffs_conf_t conf = {.base_path = FLASH_FS_MOUNT_PATH,
                                .partition_label = "internal",
                                .max_files = 5,
                                .format_if_mount_failed = true};

  if (!esp_spiffs_mounted(conf.partition_label)) {
    flash_fs_show_event(FLASH_FS_MOUNTING_EV, NULL);
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
      esp_spiffs_info(conf.partition_label, &flash_fs_size, &flash_fs_used);
      if (flash_fs_mutex) {
        xSemaphoreGiveRecursive(flash_fs_mutex);
      }
      flash_fs_show_event(FLASH_FS_RESULT_EV, (void*) (intptr_t) ESP_OK);
      return ESP_OK;
    }
    if (flash_fs_mutex) {
      xSemaphoreGiveRecursive(flash_fs_mutex);
    }
    flash_fs_show_event(FLASH_FS_RESULT_EV, (void*) (intptr_t) ret);
    return ret;
  }

  // Already mounted: update stats
  esp_spiffs_info(conf.partition_label, &flash_fs_size, &flash_fs_used);
  if (flash_fs_mutex) {
    xSemaphoreGiveRecursive(flash_fs_mutex);
  }
  return ESP_OK;
}

esp_err_t flash_fs_unmount(void) {
  flash_fs_init_mutex_if_needed();
  if (flash_fs_mutex &&
      xSemaphoreTakeRecursive(flash_fs_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (esp_spiffs_mounted("internal")) {
    esp_err_t ret = esp_vfs_spiffs_unregister("internal");
    if (ret == ESP_OK) {
      flash_fs_size = 0;
      flash_fs_used = 0;
    }
    if (flash_fs_mutex) {
      xSemaphoreGiveRecursive(flash_fs_mutex);
    }
    return ret;
  }

  if (flash_fs_mutex) {
    xSemaphoreGiveRecursive(flash_fs_mutex);
  }
  return ESP_OK;
}

esp_err_t flash_fs_get_info(size_t* total_bytes, size_t* used_bytes) {
  flash_fs_init_mutex_if_needed();
  if (flash_fs_mutex &&
      xSemaphoreTakeRecursive(flash_fs_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (esp_spiffs_mounted("internal")) {
    esp_spiffs_info("internal", &flash_fs_size, &flash_fs_used);
  }

  if (total_bytes != NULL) {
    *total_bytes = flash_fs_size;
  }
  if (used_bytes != NULL) {
    *used_bytes = flash_fs_used;
  }

  if (flash_fs_mutex) {
    xSemaphoreGiveRecursive(flash_fs_mutex);
  }
  return ESP_OK;
}
