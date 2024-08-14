#include "menus_module.h"

#include <string.h>

#include "menus_screens.h"
#include "oled_screen.h"

menus_manager_t* menus_ctx;

static void update_menus() {
  if (menus_ctx->submenus_idx != NULL) {
    free(menus_ctx->submenus_idx);
  }
  menus_ctx->submenus_idx = NULL;
  menus_ctx->submenus_count = 0;
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (menus[i].is_visible && menus[i].parent_idx == menus_ctx->current_menu) {
      menus_ctx->submenus_count++;
    }
  }
  printf("Count: %d\n", menus_ctx->submenus_count);
  menus_ctx->submenus_idx = malloc(menus_ctx->submenus_count);
  uint8_t idx = 0;
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (menus[i].is_visible && menus[i].parent_idx == menus_ctx->current_menu) {
      menus_ctx->submenus_idx[idx++] = &menus[i].menu_idx;
      printf("%s\n", menus[i].display_name);
    }
  }
}

static void display_menus() {
  menus_screens_display_menus(menus_ctx);
}

static void refresh_menus() {
  update_menus();
  display_menus();
}

void menus_module_begin() {
  menus_ctx = malloc(sizeof(menus_manager_t));
  memset(menus_ctx, 0, sizeof(menus_manager_t));
  menus_ctx->menus_count = sizeof(menus) / sizeof(menu_t);
  printf("Menus Count: %d\n", menus_ctx->menus_count);
  oled_screen_begin();
  refresh_menus();
}