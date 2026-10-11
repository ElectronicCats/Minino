#include "menu_gfx.h"

#include <stdint.h>
#include <string.h>

#include "font8x8_basic.h"
#include "oled_screen.h"

static inline void px(int x, int y, bool lit) {
  /* oled_screen_draw_pixel: invert=true clears, false lights. */
  oled_screen_draw_pixel(x, y, !lit);
}

int menu_gfx_text(int x, int y, const char* s, int scale, bool lit) {
  for (; *s; s++) {
    unsigned char c = (unsigned char) *s;
    if (c >= 128) {
      c = '?';
    }
    const uint8_t* g = font8x8_basic_tr[c];
    for (int col = 0; col < 8; col++) {
      for (int b = 0; b < 8; b++) {
        if (g[col] & (1 << b)) {
          for (int dx = 0; dx < scale; dx++) {
            for (int dy = 0; dy < scale; dy++) {
              px(x + col * scale + dx, y + b * scale + dy, lit);
            }
          }
        }
      }
    }
    x += 8 * scale;
  }
  return x;
}

int menu_gfx_text_width(const char* s, int scale) {
  return (int) strlen(s) * 8 * scale;
}

void menu_gfx_text_clip(int x, int y, const char* s, int maxpx, bool lit) {
  char buf[24];
  int n = maxpx / 8;
  if (n > 23) {
    n = 23;
  }
  int l = (int) strlen(s);
  if (l < n) {
    n = l;
  }
  memcpy(buf, s, n);
  buf[n] = '\0';
  menu_gfx_text(x, y, buf, 1, lit);
}

void menu_gfx_frame(int x, int y, int w, int h) {
  oled_screen_draw_line(x + 1, y, x + w - 2, y, OLED_DISPLAY_NORMAL);
  oled_screen_draw_line(x + 1, y + h - 1, x + w - 2, y + h - 1,
                        OLED_DISPLAY_NORMAL);
  oled_screen_draw_line(x, y + 1, x, y + h - 2, OLED_DISPLAY_NORMAL);
  oled_screen_draw_line(x + w - 1, y + 1, x + w - 1, y + h - 2,
                        OLED_DISPLAY_NORMAL);
}

void menu_gfx_scrollbar(int n, int rows, int top) {
  if (n <= rows) {
    return;
  }
  oled_screen_draw_rect(124, 11, 3, 52, OLED_DISPLAY_NORMAL);
  int kh = 52 * rows / n;
  int ky = 11 + 52 * top / n;
  oled_screen_draw_line(125, ky, 125, ky + kh, OLED_DISPLAY_NORMAL);
}
