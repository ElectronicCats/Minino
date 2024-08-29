#pragma once

#include "nmea_parser.h"

#define GPS_ACCURACY \
  1.5  // According to the ZHONGKEWEI ATGM336H-6N-74 datasheet

// Callback
typedef void (*gps_event_callback_t)(gps_t* gps);

/**
 * @brief Initialize the GPS module
 *
 * @return void
 */
void gps_module_begin();

/**
 * @brief Start reading the GPS module
 *
 * @return void
 *
 * @note No need to call `gps_module_begin` before calling this function,
 * but in that case, you need to register the event handler manually using
 * `gps_module_register_cb`.
 */
void gps_module_start_scan();

/**
 * @brief Stop reading the GPS module
 *
 * @return void
 */
void gps_module_stop_read();

/**
 * @brief Get the signal strength
 *
 * @param gps The GPS module instance
 *
 * @return const char*
 */
char* gps_module_get_signal_strength(gps_t* gps);

/**
 * @brief Get the GPS module instance
 *
 * @param event_data The event data
 *
 * @return gps_t*
 */
gps_t* gps_module_get_instance(void* event_data);

/**
 * @brief Get the GPS module time zone
 *
 * @return uint8_t
 */
uint8_t gps_module_get_time_zone();

/**
 * @brief Set the GPS module time zone
 *
 * @param time_zone The time zone
 *
 * @return void
 */
void gps_module_set_time_zone(uint8_t time_zone);

/**
 * @brief Set the GPS module event callback
 *
 * @param callback The callback
 *
 * @return void
 */
void gps_module_register_cb(gps_event_callback_t callback);

/**
 * @brief Remove the GPS module event callback
 *
 * @return void
 */
void gps_module_unregister_cb();

void gps_module_general_data_run();
