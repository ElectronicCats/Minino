#pragma once

#include "esp_err.h"

#define FLASH_FS_MOUNT_PATH "/internal"

esp_err_t flash_fs_mount();
