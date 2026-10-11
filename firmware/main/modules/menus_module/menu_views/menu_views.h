#pragma once

#include "menus.h"

/* Menu view framework.
   A "view" is a render-only strategy for the menu screen. The user picks the
   active view in Settings; it is persisted in preferences under MENU_VIEW_KEY.
   Navigation, the menu tree and app entry/exit are unaffected - a view only
   decides how the current menu (ctx) is drawn. */

#define MENU_VIEW_KEY "menu_view" /* preferences uint8, default 0 = Classic */

typedef void (*menu_view_render_fn)(menus_manager_t* ctx);

/* Number of registered views. */
uint8_t menu_views_count(void);

/* Display name of view i (for the Settings picker). NULL if out of range. */
const char* menu_views_name(uint8_t i);

/* Active view index, read from preferences and clamped to a valid range. */
uint8_t menu_views_get_active(void);

/* Persist the active view index (clamped). */
void menu_views_set_active(uint8_t i);

/* Render the given menu with the active view. Falls back to Classic if the
   stored index is invalid. */
void menu_views_render(menus_manager_t* ctx);
