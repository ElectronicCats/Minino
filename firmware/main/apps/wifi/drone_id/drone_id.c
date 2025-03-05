#include "drone_id.h"

#include "drone_id_preferences.h"
#include "odrone_id.h"

void drone_id_begin() {
  drone_id_preferences_t* prefs = drone_id_preferences_get();
  uint8_t num_drones = prefs->num_drones;
  uint8_t channel = prefs->channel;
  float latitude = prefs->latitude;
  float longitude = prefs->longitude;
  odrone_id_begin(num_drones, channel, latitude, longitude);
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
