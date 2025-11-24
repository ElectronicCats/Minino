#ifndef _GPS_HW_H_
#define _GPS_HW_H_

#include <stdbool.h>
#include <stdint.h>
#include "gps_module.h"

#define GPS_ENABLED_MEM "gps_enabled"

enum {
  GPS_INIT_ALL,
  GPS_INIT_RADIO_ONLY,
  GPS_INIT_ADVANCED_ONLY,
  GPS_INIT_UPDATERATE_ONLY,
  GPS_INIT_AGNSS_ONLY,
  GPS_INIT_POWER_ONLY,
};

// Basic GPS hardware control
void gps_hw_init(void);
void gps_hw_on(void);
void gps_hw_off(void);
void gsp_hw_save_state(void);
bool gps_hw_get_state(void);

// GPS configuration functions
void gps_hw_configure_options(uint8_t init_type);
void gps_hw_reset_advanced_config(void);
void gps_hw_set_update_rate(uint8_t rate);
void gps_hw_enable_agnss(void);
void gps_hw_disable_agnss(void);
void gps_hw_set_power_mode(gps_power_mode_t mode);

// UART initialization
bool gps_hw_init_preferences(uint8_t init_type);

#endif  // _GPS_HW_H_