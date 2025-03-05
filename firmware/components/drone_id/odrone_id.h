#ifndef _ODRONE_ID_H_
#define _ODRONE_ID_H_

#include <stdio.h>

void set_wifi_ap(char* ssid, uint8_t wifi_channel);

#ifdef __cplusplus
extern "C" {
#endif

void odrone_id_begin(uint8_t num_drones,
                     uint8_t channel,
                     float latitude,
                     float longitude);
void odrone_id_set_wifi_ap(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif  // _ODRONE_ID_H_