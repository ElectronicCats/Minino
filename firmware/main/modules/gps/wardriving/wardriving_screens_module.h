#pragma once

/**
 * @brief Show the help screen
 *
 * @return void
 */
void wardriving_screens_show_help();

/**
 * @brif Display the scanning wifi networks animation
 *
 * @return void
 */
void wardriving_screens_wifi_animation_task();

/**
 * @brief Display the scanning screen
 *
 * @param packets Number of packets received
 * @param signal Signal strength
 *
 * @return void
 */
void wardriving_screens_module_scanning(uint32_t packets, char* signal);

/**
 * @brief Display the loading text screen
 *
 * @return void
 */
void wardriving_screens_module_loading_text();

/**
 * @brief Display the no SD card screen
 *
 * @return void
 */
void wardriving_screens_module_no_sd_card();

/**
 * @brief Display the format SD card screen
 *
 * @return void
 */
void wardriving_screens_module_format_sd_card();

/**
 * @brief Display the no GPS signal screen
 *
 * @return void
 */
void wardriving_screens_module_no_gps_signal();

/**
 * @brief Display the formating SD card screen
 *
 * @return void
 */
void wardriving_screens_module_formating_sd_card();

/**
 * @brief Display the failed format SD card screen
 *
 * @return void
 */
void wardriving_screens_module_failed_format_sd_card();
