#ifndef _DRONE_ID_H_
#define _DRONE_ID_H_

#include <stdbool.h>
#include <stdio.h>

void drone_id_begin();
void drone_id_set_location_source(uint8_t location_source);
void drone_id_set_num_spoofers(uint8_t num_drones);
void drone_id_set_wifi_ap(uint8_t channel);
void drone_id_set_location(float latitude, float longitude);
void drone_id_set_ble_drone(uint8_t ble_drone);

#endif  // _DRONE_ID_H_