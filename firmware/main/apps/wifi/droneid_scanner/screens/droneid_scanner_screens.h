#ifndef __DRONEID_SCANNER_SCREENS_H__
#define __DRONEID_SCANNER_SCREENS_H__

#include <string.h>
#include "droneid_scanner.h"
#include "esp_wifi.h"
#include "opendroneid.h"

#define MAX_DRONEID_PACKETS 10

void droneid_scanner_screen_main(void);
void droneid_scanner_update_list(uint8_t* mac, uav_data* uas_data);

#endif