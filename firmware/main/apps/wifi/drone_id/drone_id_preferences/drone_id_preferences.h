#ifndef _DRONE_ID_PREFERENCES_H_
#define _DRONE_ID_PREFERENCES_H_

#include <stdbool.h>
#include <stdio.h>

typedef enum {
  DRONE_PREF_LOCATION_SOURCE_GPS,
  DRONE_PREF_LOCATION_SOURCE_MANUAL
} drone_pref_location_source_e;

typedef struct {
  uint8_t num_drones;
  uint8_t channel;
  drone_pref_location_source_e location_source;
  float latitude;
  float longitude;
  bool add_ble_drone;
} drone_id_preferences_t;

void drone_id_preferences_begin();
void drone_id_preferences_save();
void drone_id_preferences_set_num_drones(uint8_t num_drones);
void drone_id_preferences_set_channel(uint8_t channel);
void drone_id_preferences_set_location_source(
    drone_pref_location_source_e location_source);
void drone_id_preferences_set_latitude(float latitude);
void drone_id_preferences_set_longitude(float longitude);
drone_id_preferences_t* drone_id_preferences_get();
void drone_id_preferences_set_add_ble_drone(bool add_ble_drone);

#endif  //_DRONE_ID_PREFERENCES_H_