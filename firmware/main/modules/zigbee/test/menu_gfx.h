#pragma once
#include <stdbool.h>
#include <string.h>
static inline void menu_gfx_text(int x, int y, const char* s, int sc, bool c) {
  (void) x;
  (void) y;
  (void) s;
  (void) sc;
  (void) c;
}
static inline void menu_gfx_text_clip(int x,
                                      int y,
                                      const char* s,
                                      int max_w,
                                      bool c) {
  (void) x;
  (void) y;
  (void) s;
  (void) max_w;
  (void) c;
}
static inline int menu_gfx_text_width(const char* s, int sc) {
  return (int) strlen(s) * 8 * sc;
}
