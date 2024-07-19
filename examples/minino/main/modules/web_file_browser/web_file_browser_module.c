#include "web_file_browser_module.h"

#include "keyboard_module.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "web_file_browser.h"
#include "web_file_browser_screens.h"
#include "wifi_ap.h"

static void web_file_browser_input_cb(button_event_t button_pressed);
static void web_file_browser_module_exit();

void web_file_browser_module_init() {
  oled_screen_clear();
  menu_screens_set_app_state(true, web_file_browser_input_cb);
  if (sd_card_mount() == ESP_OK) {
    wifi_ap_init();
    web_file_browser_set_show_event_cb(web_file_browse_show_event_handler);
    web_file_browser_init();
  } else {
    oled_screen_display_text("     SD Card    ", 0, 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_text("  Mount Failed  ", 0, 4, OLED_DISPLAY_NORMAL);
    vTaskDelay(pdMS_TO_TICKS(2000));
    web_file_browser_module_exit();
  }
}
void web_file_browser_module_exit() {
  screen_module_set_screen(MENU_WEB_SD_BROWSER);
  esp_restart();
}
static void web_file_browser_input_cb(button_event_t button_pressed) {
  uint8_t button_name = button_pressed >> 4;
  uint8_t button_event = button_pressed & 0x0F;
  if (button_event != BUTTON_SINGLE_CLICK) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      web_file_browser_module_exit();
      break;
    case BUTTON_RIGHT:
      break;
    case BUTTON_UP:
      break;
    case BUTTON_DOWN:
      break;
    default:
      break;
  }
}