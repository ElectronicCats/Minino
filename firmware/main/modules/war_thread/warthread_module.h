#pragma once
#include <stdint.h>

typedef struct {
  char* session_str;
  uint16_t session_records_count;
} thread_module_t;

/**
 * @brief Initialize the wardriving thread module
 */
void warthread_module_begin(void);

/**
 * @brief Stop the wardriving thread module
 */
void warthread_module_exit(void);
