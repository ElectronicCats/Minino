#include "drone_id_screens.h"
#include "drone_id_preferences.h"
#include "drone_id_scenes.h"
#include "menus_module.h"
#include "oled_screen.h"

static void main_screen_input_cb(uint8_t button, uint8_t event) {
  if (button != BUTTON_LEFT && event != BUTTON_PRESS_DOWN) {
    return;
  } else if (button == BUTTON_UP || button == BUTTON_DOWN ||
             button == BUTTON_RIGHT) {
    return;  // No hacer nada
  }
  drone_id_scenes_main();
}

void drone_id_screens_main_draw_cb(float latitude, float longitude) {
  if (drone_id_scenes_get_current() != DRONE_SCENES_RUN) {
    return;
  }

  drone_id_preferences_t prefs = *drone_id_preferences_get();
  if (prefs.location_source == DRONE_PREF_LOCATION_SOURCE_MANUAL) {
    latitude = prefs.latitude;
    longitude = prefs.longitude;
  }
  char str[20];
  oled_screen_clear_buffer();
  sprintf(str, "Ch: %d", prefs.channel);
  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  sprintf(str, "Qty: %d", prefs.num_drones);
  oled_screen_display_text(str, 0, 1, OLED_DISPLAY_NORMAL);
  sprintf(str, "Lat: %f", latitude);
  oled_screen_display_text(str, 0, 2, OLED_DISPLAY_NORMAL);
  sprintf(str, "Lon: %f", longitude);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

void drone_id_screens_main() {
  menus_module_set_app_state(true, main_screen_input_cb);
  drone_id_preferences_t prefs = *drone_id_preferences_get();
  drone_id_screens_main_draw_cb(prefs.latitude, prefs.longitude);
}
