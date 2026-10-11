#include "menu_style.h"

#include "general_radio_selection.h"
#include "menu_views.h"
#include "menus_module.h"

/* Filled from the view registry on open. Bounded to a small fixed max. */
#define MENU_STYLE_MAX_VIEWS 8
static const char* s_options[MENU_STYLE_MAX_VIEWS];

static void menu_style_selection_handler(uint8_t choice) {
  menu_views_set_active(choice);
}

void menu_style_open_menu(void) {
  uint8_t count = menu_views_count();
  if (count > MENU_STYLE_MAX_VIEWS) {
    count = MENU_STYLE_MAX_VIEWS;
  }
  for (uint8_t i = 0; i < count; i++) {
    s_options[i] = menu_views_name(i);
  }

  general_radio_selection_menu_t menu;
  menu.banner = "Menu Style";
  menu.exit_cb = menus_module_exit_app;
  menu.options = s_options;
  menu.options_count = count;
  menu.style = RADIO_SELECTION_OLD_STYLE;
  menu.current_option = menu_views_get_active();
  menu.select_cb = menu_style_selection_handler;
  general_radio_selection(menu);
}
