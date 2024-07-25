#pragma once

/**
 * @brief Function to show the wardriving scanning screen
 *
 * @param packets Number of packets received
 * @param signal Signal strength
 *
 * @return void
 */
void wardriving_screens_module_scanning(uint32_t packets, char* signal);

/**
 * @brief Function to show the wardriving loading text screen
 *
 * @return void
 */
void wardriving_screens_module_loading_text();
