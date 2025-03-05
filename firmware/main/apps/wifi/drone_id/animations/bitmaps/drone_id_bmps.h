#ifndef _TAMA_BMPS_H_
#define _TAMA_BMPS_H_

#include <stdio.h>

#include "bitmap_t.h"

const unsigned char drone_id_bmp_1_16x8[] = {0x00, 0x00, 0x20, 0x04, 0x23, 0xc4,
                                             0x27, 0xe4, 0x3f, 0xfc, 0x17, 0xe8,
                                             0x10, 0x08, 0x00, 0x00};
const unsigned char drone_id_bmp_2_16x8[] = {0x00, 0x00, 0x20, 0x04, 0x73, 0xce,
                                             0x27, 0xe4, 0x3f, 0xfc, 0x17, 0xe8,
                                             0x10, 0x08, 0x00, 0x00};
const unsigned char drone_id_bmp_3_16x8[] = {0x00, 0x00, 0x20, 0x04, 0xfb, 0xdf,
                                             0x27, 0xe4, 0x3f, 0xfc, 0x17, 0xe8,
                                             0x10, 0x08, 0x00, 0x00};

const bitmap_t drone_id_bitmaps[3] = {{drone_id_bmp_1_16x8, 16, 8},
                                      {drone_id_bmp_2_16x8, 16, 8},
                                      {drone_id_bmp_3_16x8, 16, 8}};

#endif  // _TAMA_BMPS_H_