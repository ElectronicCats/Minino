#pragma once

typedef struct {
  char* session_str;
} warbee_module_t;

/**
 * @brief Initialize the wardriving zigbee module
 *
 * @return void
 */
void warbee_module_begin();

void warbee_module_exit();