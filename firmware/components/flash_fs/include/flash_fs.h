#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define FLASH_FS_MOUNT_PATH "/internal"

typedef enum { FLASH_FS_MOUNTING_EV, FLASH_FS_RESULT_EV } flash_fs_events_t;

void flash_fs_begin(void* screens_cb);
esp_err_t flash_fs_mount(void);
esp_err_t flash_fs_unmount(void);
bool flash_fs_is_mounted(void);
esp_err_t flash_fs_get_info(size_t* total_bytes, size_t* used_bytes);
