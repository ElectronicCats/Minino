#include "modbus_dos_screens.h"

#include "menus_module.h"
#include "oled_screen.h"

#include "modbus_dos_prefs.h"
#include "modbus_dos_scenes.h"

static void main_screen_input_cb(uint8_t button, uint8_t event) {
  if (button != BUTTON_LEFT || event != BUTTON_PRESS_DOWN) {
    return;
  }
  modbus_dos_scenes_main();
}

static void modbus_dos_screens_main_draw_cb() {
  modbus_dos_prefs_t prefs = *modubs_dos_prefs_get_prefs();
  oled_screen_clear_buffer();
  char str[20];
  oled_screen_display_text(prefs.ssid, 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(prefs.pass, 0, 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(prefs.ip, 0, 2, OLED_DISPLAY_NORMAL);
  sprintf(str, "Port: %d", prefs.port);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

void modbus_dos_screens_main() {
  menus_module_set_app_state(true, main_screen_input_cb);
  modbus_dos_screens_main_draw_cb();
}
