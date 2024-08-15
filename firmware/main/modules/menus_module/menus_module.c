#include "menus_module.h"

#include <string.h>

#include "keyboard_module.h"
#include "menu_screens_modules.h"
#include "menus_screens.h"
#include "oled_screen.h"

menus_manager_t* menus_ctx;
static void menus_input_cb(uint8_t button_name, uint8_t button_event);

static uint8_t get_menu_idx(menu_idx_t menu_idx) {
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (menus[i].menu_idx == menu_idx) {
      return i;
    }
  }
  return 0;
}

static void update_menus() {
  if (menus_ctx->submenus_idx != NULL) {
    for (uint8_t i = 0; i < menus_ctx->submenus_count; i++) {
      free(menus_ctx->submenus_idx[i]);
    }
    free(menus_ctx->submenus_idx);
  }
  menus_ctx->submenus_idx = NULL;
  menus_ctx->submenus_count = 0;
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (menus[i].is_visible && menus[i].parent_idx == menus_ctx->current_menu) {
      menus_ctx->submenus_count++;
    }
  }
  if (!menus_ctx->submenus_count) {
    return;
  }
  menus_ctx->submenus_idx =
      malloc(menus_ctx->submenus_count * sizeof(uint8_t*));
  uint8_t submenu_idx = 0;
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (menus[i].is_visible && menus[i].parent_idx == menus_ctx->current_menu) {
      menus_ctx->submenus_idx[submenu_idx] = malloc(sizeof(uint8_t));
      *menus_ctx->submenus_idx[submenu_idx] = i;
      submenu_idx++;
    }
  }
}

static void display_menus() {
  menus_screens_display_menus_f(menus_ctx);
}

static void refresh_menus() {
  update_menus();
  menus_ctx->selected_submenu =
      menus[get_menu_idx(menus_ctx->current_menu)].last_selected_submenu;
  display_menus();
}
static void navigation_up() {
  menus_ctx->selected_submenu = menus_ctx->selected_submenu == 0
                                    ? menus_ctx->submenus_count - 1
                                    : menus_ctx->selected_submenu - 1;
  display_menus();
}
static void navigation_down() {
  menus_ctx->selected_submenu =
      ++menus_ctx->selected_submenu < menus_ctx->submenus_count
          ? menus_ctx->selected_submenu
          : 0;
  display_menus();
}

static void navigation_enter() {
  if (!menus_ctx->submenus_count) {
    return;
  }
  menus[get_menu_idx(menus_ctx->current_menu)].last_selected_submenu =
      menus_ctx->selected_submenu;
  menus_ctx->current_menu =
      menus[*menus_ctx->submenus_idx[menus_ctx->selected_submenu]].menu_idx;
  refresh_menus();
  void (*cb)() = menus[get_menu_idx(menus_ctx->current_menu)].on_enter_cb;
  if (cb) {
    cb();
  }
}

static void navigation_exit() {
  if (menus_ctx->current_menu == MENU_MAIN_2) {
    return;
  }
  menus[get_menu_idx(menus_ctx->current_menu)].last_selected_submenu =
      menus_ctx->selected_submenu;
  void (*cb)() = menus[get_menu_idx(menus_ctx->current_menu)].on_exit_cb;
  if (cb) {
    cb();
  }
  menus_ctx->current_menu =
      menus[get_menu_idx(menus_ctx->current_menu)].parent_idx;
  refresh_menus();
}

static void menus_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      navigation_exit();
      break;
    case BUTTON_RIGHT:
      navigation_enter();
      break;
    case BUTTON_UP:
      navigation_up();
      break;
    case BUTTON_DOWN:
      navigation_down();
      break;
    default:
      break;
  }
}

void menus_module_begin() {
  menus_ctx = malloc(sizeof(menus_manager_t));
  memset(menus_ctx, 0, sizeof(menus_manager_t));
  menus_ctx->menus_count = sizeof(menus) / sizeof(menu_t);
  printf("Menus Count: %d\n", menus_ctx->menus_count);
  menu_screens_set_app_state(true, menus_input_cb);
  oled_screen_begin();
  refresh_menus();
}