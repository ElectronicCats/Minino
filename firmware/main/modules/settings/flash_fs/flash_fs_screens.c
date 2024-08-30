#include "flash_fs_screens.h"

#include "keyboard_module.h"
#include "modals_module.h"

static void show_mounting_banner() {
  modals_module_show_info(
      "Mounting Flash", "Mounting Flash File System, please wait", 2000, false);
}

static void show_result_banner(esp_err_t err) {
  if (err != ESP_OK) {
    modals_module_show_info("ERROR", esp_err_to_name(err), 2000, false);
  }
}

void flash_fs_screens_handler(flash_fs_events_t event, void* ctx) {
  switch (event) {
    case FLASH_FS_MOUNTING_EV:
      show_mounting_banner();
      break;
    case FLASH_FS_RESULT_EV:
      show_result_banner(ctx);
      break;
    default:
      break;
  }
}