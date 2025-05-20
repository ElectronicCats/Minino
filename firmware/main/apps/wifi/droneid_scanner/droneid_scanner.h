#ifndef __DRONEID_SCANNER_H__
#define __DRONEID_SCANNER_H__

#include "opendroneid.h"

#define BEACON_OFFSET        36
#define BEACON_PACKET_OFFSET 7

typedef struct {
  uint8_t mac[6];
  uint8_t padding[1];
  int8_t rssi;
  char op_id[ODID_ID_SIZE + 1];
  char uav_id[ODID_ID_SIZE + 1];
  double lat_d;
  double long_d;
  double base_lat_d;
  double base_long_d;
  int altitude_msl;
  int height_agl;
  int speed;
  int heading;
  int speed_vertical;
  int altitude_pressure;
  int horizontal_accuracy;
  int vertical_accuracy;
  int baro_accuracy;
  int speed_accuracy;
  int timestamp;
  int status;
  int height_type;
  int operator_location_type;
  int classification_type;
  int area_count;
  int area_radius;
  int area_ceiling;
  int area_floor;
  int operator_altitude_geo;
  uint32_t system_timestamp;
  int operator_id_type;
  uint8_t ua_type;
  uint8_t auth_type;
  uint8_t auth_page;
  uint8_t auth_length;
  uint32_t auth_timestamp;
  char auth_data[ODID_AUTH_PAGE_NONZERO_DATA_SIZE + 1];

  uint8_t desc_type;
  char description[ODID_STR_SIZE + 1];
  uint16_t channel;
} uav_data;

void droneid_scanner_begin(void);

#endif