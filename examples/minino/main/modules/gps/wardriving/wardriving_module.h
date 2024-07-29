#pragma once

/**
 * @brief Initialize the wardriving module
 *
 * @return void
 */
void wardriving_module_begin();

/**
 * End the wardriving module
 *
 * @return void
 */
void wardriving_module_end();

/**
 * Start scanning
 *
 * @return void
 */
void wardriving_module_start_scan();

/**
 * Stop reading the scan
 *
 * @return void
 */
void wardriving_module_stop_scan();
