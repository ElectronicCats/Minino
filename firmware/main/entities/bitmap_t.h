#ifndef _BITMAP_T_H
#define _BITMAP_T_H

#include <stdio.h>

typedef struct {
  const unsigned char* bitmap;
  uint8_t width;
  uint8_t height;
} bitmap_t;

#endif  // _BITMAP_T_H