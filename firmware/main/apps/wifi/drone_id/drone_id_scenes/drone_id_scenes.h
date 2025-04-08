#ifndef _ODRONE_ID_SCENES_H_
#define _ODRONE_ID_SCENES_H_

#include <stdio.h>

typedef enum {
  DRONE_SCENES_MAIN,
  DRONE_SCENES_RUN,
  DRONE_SCENES_SETTINGS,
  DRONE_SCENES_SETTINGS_NUM_DRONES,
  DRONE_SCENES_SETTINGS_CHANNEL,
  DRONE_SCENES_SETTINGS_LOCATION_SRC,
  DRONE_SCENES_SETTINGS_BLE_DRONE,
  DRONE_SCENES_SETTINGS_HELP,
  DRONE_SCENES_HELP,
} drone_id_scenes_e;

void drone_id_scenes_main();
void drone_id_scenes_help();
uint8_t drone_id_scenes_get_current();

#endif  // _ODRONE_ID_SCENES_H_