#pragma once
#include <stdint.h>

/* @brief Struct for the sound context
  @struct uint16_t time in seconds
  @struct uint16_t count of the loop sound
*/
typedef struct {
  uint16_t time;
  uint16_t count;
} sound_context_t;