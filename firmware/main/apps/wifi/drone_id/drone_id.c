#include "drone_id.h"
#include "drone_id_anim.h"
#include "drone_id_preferences.h"
#include "drone_id_screens.h"
#include "drone_id_scenes/drone_id_scenes.h"
#include "gps_module.h"
#include "odrone_id.h"
#include "oled_screen.h"
#include "app_manager.h"

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

  drone_id_set_ble_drone(prefs->add_ble_drone);
  drone_anim_scan();
  drone_id_screens_main();
}

void drone_id_stop(void) {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  if (prefs->location_source == DRONE_PREF_LOCATION_SOURCE_GPS) {
    gps_module_unregister_cb();
    gps_module_stop_read();
  }
  odrone_id_stop();
}

void drone_id_set_location_source(uint8_t location_source) {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  prefs->location_source = location_source;
  drone_id_preferences_save();

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

void drone_id_set_ble_drone(uint8_t ble_drone) {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  prefs->add_ble_drone = ble_drone;
  drone_id_preferences_save();

  odrone_id_set_ble_drone(ble_drone);
}


// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void drone_id_on_enter(void) {
  drone_id_scenes_main();
}

static void drone_id_on_input(uint8_t button_name, uint8_t button_event) {
  // Scenes install their own input handlers
  (void)button_name;
  (void)button_event;
}

static void drone_id_on_exit(void) {
  // Stop drone ID if running
  drone_id_stop();
}

// Wrapper for menu entry
void app_manager_enter_wrapper_drone_id(void) {
  extern const minino_app_t drone_id_app;
  app_manager_enter(&drone_id_app);
}

// App definition for app_manager
const minino_app_t drone_id_app = {
  .name = "drone_id",
  .on_enter = drone_id_on_enter,
  .on_input = drone_id_on_input,
  .on_exit = drone_id_on_exit,
};

