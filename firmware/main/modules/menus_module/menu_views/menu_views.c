#include "menu_views.h"

#include <stdio.h>
#include <string.h>

#include "menu_gfx.h"
#include "menu_icons.h"
#include "menus_screens.h"
#include "oled_screen.h"
#include "preferences.h"

/* Accessors over the menu manager context. */
#define ITEM_NAME(ctx, i) (menus[*(ctx)->submenus_idx[(i)]].display_name)

static const char* current_title(menus_manager_t* ctx) {
  if (ctx->current_menu == MENU_MAIN) {
    return "MININO";
  }
  for (uint8_t i = 0; i < ctx->menus_count; i++) {
    if (menus[i].menu_idx == ctx->current_menu) {
      return menus[i].display_name;
    }
  }
  return "";
}

static void header(menus_manager_t* ctx) {
  menu_gfx_text_clip(2, 0, current_title(ctx), 88, true);
  char pg[12];
  snprintf(pg, sizeof pg, "%d/%d", ctx->selected_submenu + 1,
           ctx->submenus_count);
  menu_gfx_text(128 - menu_gfx_text_width(pg, 1), 0, pg, 1, true);
  oled_screen_draw_line(0, 9, 127, 9, OLED_DISPLAY_NORMAL);
}

/* ---- VIEW: Classic (unchanged existing renderer) ----------------------- */
static void render_classic(menus_manager_t* ctx) {
  menus_screens_display_menus_f(ctx);
}

/* ---- VIEW: Icon List (icon + label, outline selection, scrolls) -------- */
static void render_icon_list(menus_manager_t* ctx) {
  oled_screen_clear_buffer();
  if (!ctx->submenus_count) {
    oled_screen_display_show();
    return;
  }
  header(ctx);
  const int rows = 4, rh = 13, y0 = 11;
  int sel = ctx->selected_submenu, n = ctx->submenus_count;
  int top = (sel >= rows) ? sel - rows + 1 : 0;
  for (int r = 0; r < rows && top + r < n; r++) {
    int i = top + r, y = y0 + r * rh;
    if (i == sel) {
      menu_gfx_frame(0, y - 1, 120, rh - 1);
    }
    menu_icon_for(ITEM_NAME(ctx, i))(3, y);
    menu_gfx_text_clip(18, y + 1, ITEM_NAME(ctx, i), 100, true);
  }
  menu_gfx_scrollbar(n, rows, top);
  oled_screen_display_show();
}

/* ---- VIEW: Grid (icon-only 4x2, paginated) ----------------------------- */
static void render_grid(menus_manager_t* ctx) {
  oled_screen_clear_buffer();
  if (!ctx->submenus_count) {
    oled_screen_display_show();
    return;
  }
  int sel = ctx->selected_submenu, n = ctx->submenus_count;
  menu_gfx_text_clip(2, 0, ITEM_NAME(ctx, sel), 88, true);
  char pg[12];
  snprintf(pg, sizeof pg, "%d/%d", sel + 1, n);
  menu_gfx_text(128 - menu_gfx_text_width(pg, 1), 0, pg, 1, true);
  oled_screen_draw_line(0, 9, 127, 9, OLED_DISPLAY_NORMAL);
  const int per = 8, cols = 4, cw = 32, rh = 26, y0 = 12;
  int base = (sel / per) * per;
  for (int k = 0; k < per && base + k < n; k++) {
    int i = base + k, cx = (k % cols) * cw, cy = y0 + (k / cols) * rh;
    if (i == sel) {
      menu_gfx_frame(cx + 4, cy + 1, cw - 8, rh - 4);
      menu_gfx_frame(cx + 5, cy + 2, cw - 10, rh - 6);
    }
    menu_icon_for(ITEM_NAME(ctx, i))(cx + cw / 2 - 6, cy + (rh - 12) / 2 - 1);
  }
  oled_screen_display_show();
}

/* ---- VIEW: Hero + Dock -------------------------------------------------- */
static void render_hero(menus_manager_t* ctx) {
  oled_screen_clear_buffer();
  if (!ctx->submenus_count) {
    oled_screen_display_show();
    return;
  }
  int sel = ctx->selected_submenu, n = ctx->submenus_count;
  const char* name = ITEM_NAME(ctx, sel);
  menu_icon_for(name)(6, 8);
  int sc = (menu_gfx_text_width(name, 2) <= 104) ? 2 : 1;
  menu_gfx_text(24 + (104 - menu_gfx_text_width(name, sc)) / 2, 4, name, sc,
                true);
  char pos[16];
  snprintf(pos, sizeof pos, "%d of %d", sel + 1, n);
  menu_gfx_text(24, 23, pos, 1, true);
  oled_screen_draw_line(0, 34, 127, 34, OLED_DISPLAY_NORMAL);
  const int cap = 8, pitch = 16, dy = 40;
  int win = (sel < cap) ? 0 : sel - cap + 1;
  for (int k = 0; k < cap && win + k < n; k++) {
    int i = win + k, dx = k * pitch;
    if (i == sel) {
      menu_gfx_frame(dx + 1, dy - 1, 14, 22);
    }
    menu_icon_for(ITEM_NAME(ctx, i))(dx + 2, dy + 3);
  }
  oled_screen_display_show();
}

/* Registry. Index == stored preference. Classic (0) is the default. */
typedef struct {
  const char* name;
  menu_view_render_fn render;
} menu_view_t;

static const menu_view_t s_views[] = {
    {"Classic", render_classic},
    {"Icon List", render_icon_list},
    {"Grid", render_grid},
    {"Hero Dock", render_hero},
};

#define MENU_VIEWS_COUNT (sizeof(s_views) / sizeof(s_views[0]))

uint8_t menu_views_count(void) {
  return (uint8_t) MENU_VIEWS_COUNT;
}

const char* menu_views_name(uint8_t i) {
  return (i < MENU_VIEWS_COUNT) ? s_views[i].name : NULL;
}

uint8_t menu_views_get_active(void) {
  uint8_t v = (uint8_t) preferences_get_uint(MENU_VIEW_KEY, 0);
  return (v < MENU_VIEWS_COUNT) ? v : 0;
}

void menu_views_set_active(uint8_t i) {
  if (i >= MENU_VIEWS_COUNT) {
    i = 0;
  }
  preferences_put_uint(MENU_VIEW_KEY, i);
}

void menu_views_render(menus_manager_t* ctx) {
  s_views[menu_views_get_active()].render(ctx);
}
