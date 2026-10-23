#pragma once
/* Maps a menu item's display name to a 12x12 icon draw function.
   The returned function draws at pixel (x,y). Unknown names get a generic dot.
 */
typedef void (*menu_icon_fn)(int x, int y);
menu_icon_fn menu_icon_for(const char* name);
