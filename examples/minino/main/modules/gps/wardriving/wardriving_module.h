#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * Initialize the Wardriving module
 *
 * @return void
 */
void wardriving_begin();

/**
 * Start the Wardriving module
 *
 * @return void
 */
void wardriving_start();

/**
 * Stop the Wardriving module
 *
 * @return void
 */
void wardriving_end();
