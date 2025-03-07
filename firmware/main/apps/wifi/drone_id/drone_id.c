#include "drone_id.h"

#include "drone_id_anim.h"
#include "drone_id_preferences.h"
#include "drone_id_screens.h"
#include "gps_module.h"
#include "odrone_id.h"
#include "oled_screen.h"

static void gps_event_handler_cb(gps_t* gps) {
  odrone_id_set_location(gps->latitude, gps->longitude);
  drone_id_screens_main_draw_cb(gps->latitude, gps->longitude);
}

void drone_id_begin() {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  uint8_t num_drones = prefs->num_drones;
  uint8_t channel = prefs->channel;
  float latitude = prefs->latitude;
  float longitude = prefs->longitude;
  odrone_id_begin(num_drones, channel, latitude, longitude);
  if (prefs->location_source == DRONE_PREF_LOCATION_SOURCE_GPS) {
    // oled_screen_get_last_buffer();
    gps_module_register_cb(gps_event_handler_cb);
    gps_module_start_scan();
    // oled_screen_set_last_buffer();
  }
  drone_anim_scan();
  drone_id_screens_main();
}

void drone_id_set_location_source(uint8_t location_source) {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  prefs->location_source = location_source;
  drone_id_preferences_save(prefs);

  if (location_source == DRONE_PREF_LOCATION_SOURCE_GPS) {
    gps_module_register_cb(gps_event_handler_cb);
    gps_module_start_scan();
  } else {
    gps_module_unregister_cb();
    odrone_id_set_location(prefs->latitude, prefs->longitude);
  }
}

void drone_id_set_num_spoofers(uint8_t num_drones) {
  odrone_id_set_num_spoofers(num_drones);
}

void drone_id_set_wifi_ap(uint8_t channel) {
  odrone_id_set_wifi_ap(channel);
}

void drone_id_set_location(float latitude, float longitude) {
  odrone_id_set_location(latitude, longitude);
}
