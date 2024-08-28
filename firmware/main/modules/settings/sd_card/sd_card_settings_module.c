#include "esp_log.h"

#include "keyboard_module.h"
#include "menus_module.h"
#include "sd_card.h"
#include "sd_card_settings_module.h"
#include "sd_card_settings_screens_module.h"

const char* TAG = "sd_card_settings_module";

typedef enum {
  SD_CARD_SETTINGS_VERIFYING = 0,
  SD_CARD_SETTINGS_OK,
  SD_CARD_SETTINGS_FORMAT_QUESTION,
  SD_CARD_SETTINGS_FORMATTING,
  SD_CARD_SETTINGS_FORMAT_DONE,
  SD_CARD_SETTINGS_FAILED_FORMAT,
  SD_CARD_SETTINGS_NO_SD_CARD,
} sd_card_settings_state_t;

const char* sd_card_state_to_name[] = {
    "Verifying",  "OK",          "Format question",
    "Formatting", "Format done", "Failed format",
    "No SD card",
};

sd_card_settings_state_t state = SD_CARD_SETTINGS_VERIFYING;

void sd_card_settings_verify_sd_card() {
  menus_module_set_app_state(true, sd_card_settings_keyboard_cb);
  ESP_LOGI(TAG, "Verifying SD card...");
  state = SD_CARD_SETTINGS_VERIFYING;

  esp_err_t err = sd_card_mount();
  if (err == ESP_ERR_NOT_SUPPORTED) {
    state = SD_CARD_SETTINGS_FORMAT_QUESTION;
    sd_card_settings_screens_module_format_question();
  } else if (err == ESP_OK) {
    state = SD_CARD_SETTINGS_OK;
    sd_card_settings_screens_module_sd_card_ok();
  } else if (err != ESP_OK) {
    state = SD_CARD_SETTINGS_NO_SD_CARD;
    sd_card_settings_screens_module_no_sd_card();
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
      switch (state) {
        case SD_CARD_SETTINGS_FORMAT_QUESTION:
          sd_card_settings_screens_module_formating_sd_card();
          esp_err_t err = sd_card_format();
          if (err == ESP_OK) {
            ESP_LOGI(TAG, "Format done");
            state = SD_CARD_SETTINGS_FORMAT_DONE;
            sd_card_settings_screens_module_format_done();
          } else {
            state = SD_CARD_SETTINGS_FAILED_FORMAT;
            sd_card_settings_screens_module_failed_format_sd_card();
          }
          break;
        case SD_CARD_SETTINGS_OK:
          menus_module_exit_app();
          break;
        default:
          menus_module_exit_app();
          break;
      }
      break;
    default:
      break;
  }
}
