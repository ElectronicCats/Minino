#pragma once
#include <stdint.h>
#define OLED_DISPLAY_NORMAL 0
static inline void oled_screen_clear_buffer(void) {}
static inline void oled_screen_clear(void) {}
static inline void oled_screen_display_show(void) {}
static inline uint8_t oled_screen_get_pages(void) {
  return 8;
}
static inline void oled_screen_draw_hline(int x, int y, int w, int m) {
  (void) x;
  (void) y;
  (void) w;
  (void) m;
}
static inline void oled_screen_draw_rect(int x, int y, int w, int h, int m) {
  (void) x;
  (void) y;
  (void) w;
  (void) h;
  (void) m;
}
static inline void oled_screen_draw_box(int x, int y, int w, int h, int m) {
  (void) x;
  (void) y;
  (void) w;
  (void) h;
  (void) m;
}
