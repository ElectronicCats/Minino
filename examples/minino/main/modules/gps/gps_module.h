#pragma once

#include "nmea_parser.h"

/**
 * @brief Initialize the GPS module
 *
 * @return void
 */
void gps_module_begin();

/**
 * @brief Deinitialize the GPS module
 *
 * @return void
 */
void gps_module_exit();

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
