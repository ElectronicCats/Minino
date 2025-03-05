#ifndef _ANIMATION_T_H
#define _ANIMATION_T_H

#include <stdio.h>

#include "bitmap_t.h"

typedef struct {
  const bitmap_t* bitmaps;
  const uint16_t* order;
  const uint32_t* duration_ms;
  size_t frames_len;
  size_t bitmaps_len;
} animation_t;

#endif  // _ANIMATION_T_H