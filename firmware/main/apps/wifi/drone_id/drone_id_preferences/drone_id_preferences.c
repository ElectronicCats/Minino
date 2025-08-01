#include "drone_id_preferences.h"

#include "esp_err.h"
#include "esp_log.h"

#include "preferences.h"

#define NUM_DRONES_MEM      "num_dr"
#define CHANNEL_MEM         "channel_dr"
#define LOCATION_SOURCE_MEM "loc_src_dr"
#define LATITUDE_MEM        "lat_dr"
#define LONGITUDE_MEM       "lon_dr"
#define ADD_BLE_DRONE_MEM   "add_ble_dr"

drone_id_preferences_t* drone_pref_ptr = NULL;

static void preferences_not_initialized() {
  ESP_LOGE("DRONE_ID_PREFERENCES", "Preferences not initialized");
  ESP_LOGI("DRONE_ID_PREFERENCES", "Call drone_id_preferences_begin() first");
}

void drone_id_preferences_begin() {
  if (drone_pref_ptr) {
    return;
  }
  drone_pref_ptr = calloc(1, sizeof(drone_id_preferences_t));

  drone_pref_ptr->num_drones = preferences_get_uchar(NUM_DRONES_MEM, 1);
  drone_pref_ptr->channel = preferences_get_uchar(CHANNEL_MEM, 1);
  drone_pref_ptr->location_source = preferences_get_uchar(
      LOCATION_SOURCE_MEM, DRONE_PREF_LOCATION_SOURCE_GPS);
  drone_pref_ptr->latitude = preferences_get_float(LATITUDE_MEM, 0.0);
  drone_pref_ptr->longitude = preferences_get_float(LONGITUDE_MEM, 0.0);
  drone_pref_ptr->add_ble_drone =
      preferences_get_bool(ADD_BLE_DRONE_MEM, false);
}

void drone_id_preferences_save() {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return;
  }
  preferences_put_uchar(NUM_DRONES_MEM, drone_pref_ptr->num_drones);
  preferences_put_uchar(CHANNEL_MEM, drone_pref_ptr->channel);
  preferences_put_uchar(LOCATION_SOURCE_MEM, drone_pref_ptr->location_source);
  preferences_put_float(LATITUDE_MEM, drone_pref_ptr->latitude);
  preferences_put_float(LONGITUDE_MEM, drone_pref_ptr->longitude);
  preferences_put_bool(ADD_BLE_DRONE_MEM, drone_pref_ptr->add_ble_drone);
}

void drone_id_preferences_set_num_drones(uint8_t num_drones) {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return;
  }
  drone_pref_ptr->num_drones = num_drones;
  drone_id_preferences_save();
}

void drone_id_preferences_set_channel(uint8_t channel) {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return;
  }
  drone_pref_ptr->channel = channel;
  drone_id_preferences_save();
}

void drone_id_preferences_set_location_source(
    drone_pref_location_source_e location_source) {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return;
  }
  drone_pref_ptr->location_source = location_source;
  drone_id_preferences_save();
}

void drone_id_preferences_set_latitude(float latitude) {
  if (!drone_pref_ptr) {
    preferences_put_float(LATITUDE_MEM, latitude);
    return;
  }
  drone_pref_ptr->latitude = latitude;
  drone_id_preferences_save();
}

void drone_id_preferences_set_longitude(float longitude) {
  if (!drone_pref_ptr) {
    preferences_put_float(LONGITUDE_MEM, longitude);
    return;
  }
  drone_pref_ptr->longitude = longitude;
  drone_id_preferences_save();
}

void drone_id_preferences_set_add_ble_drone(bool add_ble_drone) {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return;
  }
  drone_pref_ptr->add_ble_drone = add_ble_drone;
  drone_id_preferences_save();
}

drone_id_preferences_t* drone_id_preferences_get() {
  if (!drone_pref_ptr) {
    preferences_not_initialized();
    return NULL;
  }
  return drone_pref_ptr;
}
