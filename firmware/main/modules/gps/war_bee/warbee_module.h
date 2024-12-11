#pragma once
#include <stdint.h>

typedef struct {
  char* session_str;
  uint16_t session_records_count;
} warbee_module_t;

/**
 * @brief Initialize the wardriving zigbee module
 *
 * @return void
 */
void warbee_module_begin();

void warbee_module_exit();