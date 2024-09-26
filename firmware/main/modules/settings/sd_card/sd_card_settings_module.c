#include "esp_log.h"

#include "keyboard_module.h"
#include "menus_module.h"
#include "sd_card.h"
#include "sd_card_settings_module.h"
#include "sd_card_settings_screens_module.h"

const char* TAG = "sd_card_settings_module";

typedef enum {
  SD_CARD_SETTINGS_VERIFYING = 0,
  SD_CARD_SETTINGS_MOUNT_OK,
  SD_CARD_SETTINGS_WRONG_FORMAT,
  SD_CARD_SETTINGS_FORMAT_QUESTION,
  SD_CARD_SETTINGS_FORMATTING,
  SD_CARD_SETTINGS_FORMAT_DONE,
  SD_CARD_SETTINGS_FAILED_FORMAT,
  SD_CARD_SETTINGS_NO_SD_CARD,
} sd_card_settings_state_t;

const char* sd_card_state_to_name[] = {
    "Verifying",  "Mount OK",    "Wrong format",  "Format question",
    "Formatting", "Format done", "Failed format", "No SD card",
};

sd_card_settings_state_t state = SD_CARD_SETTINGS_VERIFYING;

void sd_card_settings_verify_sd_card() {
  menus_module_set_app_state(true, sd_card_settings_keyboard_cb);
  ESP_LOGI(TAG, "Verifying SD card...");
  state = SD_CARD_SETTINGS_VERIFYING;
  bool format = false;
  if (menus_module_get_current_menu() == MENU_SETTINGS_SD_CARD_FORMAT) {
    format = true;
  }

  esp_err_t err = sd_card_mount();
  switch (err) {
    case ESP_ERR_NOT_SUPPORTED:
      state = SD_CARD_SETTINGS_WRONG_FORMAT;
      sd_card_settings_screens_module_wrong_format();
      break;
    case ESP_OK:
      if (format) {
        state = SD_CARD_SETTINGS_FORMAT_QUESTION;
        sd_card_settings_screens_module_format_question();
      } else {
        state = SD_CARD_SETTINGS_MOUNT_OK;
        sd_card_settings_screens_module_sd_card_ok();
      }
      break;
    default:
      state = SD_CARD_SETTINGS_NO_SD_CARD;
      sd_card_settings_screens_module_no_sd_card();
      break;
  }
  sd_card_unmount();
}

void sd_card_settings_keyboard_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG, "State: %s", sd_card_state_to_name[state]);
      esp_err_t err;
      switch (state) {
        case SD_CARD_SETTINGS_WRONG_FORMAT:
          sd_card_settings_screens_module_formating_sd_card();
          err = sd_card_check_format();
          if (err == ESP_OK) {
            ESP_LOGI(TAG, "Format done");
            state = SD_CARD_SETTINGS_FORMAT_DONE;
            sd_card_settings_screens_module_format_done();
          } else {
            state = SD_CARD_SETTINGS_FAILED_FORMAT;
            sd_card_settings_screens_module_failed_format_sd_card();
          }
          break;
        case SD_CARD_SETTINGS_FORMAT_QUESTION:
          sd_card_settings_screens_module_formating_sd_card();
          err = sd_card_check_format();
          if (err == ESP_OK) {
            ESP_LOGI(TAG, "Mount ok, formatting...");
            state = SD_CARD_SETTINGS_FORMATTING;
            esp_err_t err_format = sd_card_format();
            if (err_format == ESP_OK) {
              state = SD_CARD_SETTINGS_FORMAT_DONE;
              sd_card_settings_screens_module_format_done();
            } else {
              state = SD_CARD_SETTINGS_FAILED_FORMAT;
              sd_card_settings_screens_module_failed_format_sd_card();
            }
          } else {
            state = SD_CARD_SETTINGS_FAILED_FORMAT;
            sd_card_settings_screens_module_failed_format_sd_card();
          }
          break;
        case SD_CARD_SETTINGS_FORMATTING:
          // TODO: implement on LEFT button
          ESP_LOGI(TAG, "Formatting...");
          break;
        case SD_CARD_SETTINGS_MOUNT_OK:
        default:
          menus_module_exit_app();
          break;
      }
      break;
    default:
      break;
  }
}
