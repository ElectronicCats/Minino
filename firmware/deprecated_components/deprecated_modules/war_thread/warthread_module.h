#pragma once
#include <stdint.h>

typedef struct {
  char* session_str;
  uint16_t session_records_count;
} thread_module_t;

/**
 * @brief Initialize the wardriving zigbee module
 *
 * @return void
 */
void warthread_module_begin();

void warthread_module_exit();