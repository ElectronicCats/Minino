#ifndef WIFI_BITMAPS_H
#define WIFI_BITMAPS_H

/* This header and wifi_bitmaps.c select the bitmap data with
   CONFIG_RESOLUTION_128X64 / CONFIG_RESOLUTION_128X32. ESP-IDF only puts
   sdkconfig.h on the include path, it does not force-include it, so without
   this the macros were undefined at this point and every consumer silently
   picked the wrong variant (or, in wifi_bitmaps.c, none at all). */
#include "sdkconfig.h"

#define IDLE_FRAMES 5
#define IDLE_SPEED  20  // below this wpm value your animation will idle

// #define PREP_FRAMES 1 // uncomment if >1

#define TAP_FRAMES 2
#define TAP_SPEED  40  // above this wpm value typing animation to trigger

#define ANIM_FRAME_DURATION 200  // how long each frame lasts in ms
// #define SLEEP_TIMER 60000 // should sleep after this period of 0 wpm, needs
// fixing
#define ANIM_SIZE \
  636  // number of bytes in array, minimize for adequate firmware size, max is
       // 1024
// 'wifi_1', 32x32px
extern unsigned char wifi_bitmap_wifi_1[];
// 'wifi_2', 32x32px
extern unsigned char wifi_bitmap_wifi_2[];
// 'wifi_3', 32x32px
extern unsigned char wifi_bitmap_wifi_3[];
// 'wifi', 32x32px
extern unsigned char wifi_bitmap_wifi[];

// Array of all bitmaps for convenience. (Total bytes used to store imagesin =
// 576)
extern int wifi_bitmap_allArray_LEN;
extern unsigned char* wifi_bitmap_allArray[4];

// 'attack_1', 32x32px
extern unsigned char wifi_bitmap_attackattack_1[];
// 'attack_2', 32x32px
extern unsigned char wifi_bitmap_attackattack_2[];
// 'attack_3', 32x32px
extern unsigned char wifi_bitmap_attackattack_3[];
// 'attack_4', 32x32px
extern unsigned char wifi_bitmap_attackattack_4[];

// Array of all bitmaps for convenience. (Total bytes used to store images in =
// 576)
extern int wifi_bitmap_attackallArray_LEN;
extern unsigned char* wifi_bitmap_attackallArray[4];

// 'wifi-loading-*': 56x56px on 128x64 panels, 32x32px on 128x32 panels.
// Only the declarations live here; the data is in wifi_bitmaps.c, selected by
// CONFIG_RESOLUTION_*. Leaving a definition in this header gave every
// translation unit that includes it its own copy, which only linked thanks to
// -Wl,-zmuldefs and broke outright on the 128x32 board profiles.
extern const unsigned char epd_bitmap_wifi_loading_1[];
extern const unsigned char epd_bitmap_wifi_loading_2[];
extern const unsigned char epd_bitmap_wifi_loading_3[];
extern const unsigned char epd_bitmap_wifi_loading_4[];
extern const unsigned char* epd_bitmap_wifi_loading[4];
#endif  // WIFI_BITMAPS_H
