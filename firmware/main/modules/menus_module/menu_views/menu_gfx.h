#pragma once
#include <stdbool.h>
/* Small drawing helpers for the menu views, built on oled_screen primitives.
   All coordinates are pixels. Text can be placed at an arbitrary y (not just a
   page boundary) and scaled, which the base oled_screen text API cannot do. */

/* Draw text at pixel (x,y), integer scale (1,2,3...). Returns the x after the
   last glyph. lit=true draws lit pixels; lit=false clears (for inverted text).
 */
int menu_gfx_text(int x, int y, const char* s, int scale, bool lit);

/* Width in pixels of s at the given scale. */
int menu_gfx_text_width(const char* s, int scale);

/* Draw text clipped to maxpx width (truncated, no wrap) at pixel (x,y). */
void menu_gfx_text_clip(int x, int y, const char* s, int maxpx, bool lit);

/* Rounded (corner-less) outline box. */
void menu_gfx_frame(int x, int y, int w, int h);

/* Right-edge scrollbar for a list of n items, window of rows, top index. */
void menu_gfx_scrollbar(int n, int rows, int top);
