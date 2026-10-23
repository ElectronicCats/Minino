#include "menus_module.h"

#include <string.h>
#include "bitmaps_general.h"
#include "buzzer.h"
#include "esp_log.h"
#include "led_events.h"
#include "menu_views.h"
#include "menus_screens.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "screen_saver.h"
#include "sleep_mode.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define SCREEN_WIDTH  128
  #define SCREEN_HEIGHT 64
#else  // CONFIG_RESOLUTION_128X32
  #define SCREEN_WIDTH  128
  #define SCREEN_HEIGHT 32
#endif

static const char* TAG = "menus_module";

static menus_manager_t* menus_ctx;
static menu_t* catalogue;  /* cached menus_get(), valid for the whole program */
static void menus_input_cb(uint8_t button_name, uint8_t button_event);
static app_state2_t app_state = {.in_app = false,
                                 .input_callback = NULL,
                                 .input_last_callback = NULL};

static menu_t* menu_at(menu_idx_t id) {
  menu_t* menu = menus_find(id);
  return menu ? menu : &catalogue[0];  /* MENU_MAIN is always compiled in */
}

/*
 * Same lookup, but returns NULL instead of silently retargeting to MENU_MAIN.
 * Callers that *write* through the returned pointer (hide/reveal) must use
 * this: falling back to catalogue[0] would toggle the visibility of the root
 * sentinel, and callers that compute a parent from it would set
 * current_menu to -1, which the menu tree can never navigate back out of.
 */
static menu_t* menu_at_checked(menu_idx_t id) {
  menu_t* menu = menus_find(id);
  if (!menu) {
    ESP_LOGE(TAG, "Menu %d is not compiled in", id);
    return NULL;
  }
  return menu;
}

static int get_menu_idx_over_cmd(const char* entry_cmd) {
  for (uint8_t i = 0; i < menus_ctx->menus_count; i++) {
    if (!catalogue[i].entry_cmd) {
      continue;
    }
    /* Exact match: the old code compared only strlen(entry_cmd) bytes, so
       "deauth_scan" matched the "deauth" entry first and launched the wrong
       app. */
    if (!strcmp(catalogue[i].entry_cmd, entry_cmd)) {
      return i;
    }
  }
  ESP_LOGE(TAG, "Menu not found for command: %s", entry_cmd);
  return -1;
}

static uint8_t s_submenu_indices[MENU_COUNT];
static uint8_t* s_submenu_ptrs[MENU_COUNT];

static void update_menus() {
  menus_ctx->submenus_count = 0;
  for (uint8_t i = 0;
       i < menus_ctx->menus_count && menus_ctx->submenus_count < MENU_COUNT;
       i++) {
    if (catalogue[i].is_visible && catalogue[i].parent_idx == menus_ctx->current_menu) {
      s_submenu_indices[menus_ctx->submenus_count] = i;
      s_submenu_ptrs[menus_ctx->submenus_count] =
          &s_submenu_indices[menus_ctx->submenus_count];
      menus_ctx->submenus_count++;
    }
  }
  menus_ctx->submenus_idx = s_submenu_ptrs;
}

static void display_menus() {
  /* Safety: ensure submenus are populated (e.g., after wake from screensaver
     at boot where refresh_menus() wasn't called yet). */
  if (menus_ctx != NULL && menus_ctx->submenus_count == 0) {
    update_menus();
    if (menus_ctx->selected_submenu >= menus_ctx->submenus_count) {
      menus_ctx->selected_submenu =
          menus_ctx->submenus_count > 0 ? menus_ctx->submenus_count - 1 : 0;
    }
  }
  menu_views_render(menus_ctx);
  oled_screen_display_show();
}

static void refresh_menus() {
  update_menus();
  menus_ctx->selected_submenu =
      menu_at(menus_ctx->current_menu)->last_selected_submenu;
  /* last_selected_submenu comes from the catalogue and can outlive a config
     that had more children (board/profile switch without erase-flash). Clamp
     it so the renderers always have a valid selection to highlight. */
  if (menus_ctx->submenus_count == 0) {
    menus_ctx->selected_submenu = 0;
  } else if (menus_ctx->selected_submenu >= menus_ctx->submenus_count) {
    menus_ctx->selected_submenu = menus_ctx->submenus_count - 1;
  }
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
  menu_at(menus_ctx->current_menu)->last_selected_submenu =
      menus_ctx->selected_submenu;
  menus_ctx->current_menu =
      catalogue[*menus_ctx->submenus_idx[menus_ctx->selected_submenu]].menu_idx;
  menus_ctx->parent_menu_idx =
      menu_at(menus_ctx->current_menu)->parent_idx;
  refresh_menus();
  menu_cb_t cb = menu_at(menus_ctx->current_menu)->on_enter_cb;
  if (cb) {
    cb();
  }
}

static void navigation_exit() {
  if (menus_ctx->current_menu == MENU_MAIN) {
    screen_saver_run();
    return;
  }
  menu_at(menus_ctx->current_menu)->last_selected_submenu = 0;
  menu_cb_t cb = menu_at(menus_ctx->current_menu)->on_exit_cb;
  if (cb) {
    cb();
  }
  menus_ctx->current_menu = menu_at(menus_ctx->current_menu)->parent_idx;
  menus_ctx->parent_menu_idx = menu_at(menus_ctx->current_menu)->parent_idx;
  refresh_menus();
}

static void menus_input_cb(uint8_t button_name, uint8_t button_event) {
  /* menus_ctx is NULL between keyboard_module_begin() and menus_module_begin();
     input_callback is only installed at the end of menus_module_begin(), so
     this cannot be reached in that window, but guard anyway. */
  if (menus_ctx == NULL || menus_ctx->input_lock) {
    return;
  }

  if (app_state.in_app) {
    if (app_state.input_callback) {
      app_state.input_callback(button_name, button_event);
      return;
    }
    app_state.in_app = false;
    return;
  }

  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  sleep_mode_reset_timer();
  if (screen_saver_get_idle_state()) {
    update_menus();
    if (menus_ctx->selected_submenu >= menus_ctx->submenus_count) {
      menus_ctx->selected_submenu =
          menus_ctx->submenus_count > 0 ? menus_ctx->submenus_count - 1 : 0;
    }
    display_menus();
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

static void show_logo() {
  oled_screen_clear();
  led_control_run_effect(led_control_pulse_leds);
  buzzer_play_for(100);
  screen_saver_run();
}

void menus_module_set_reset_screen(menu_idx_t menu) {
  preferences_put_int("MENUNUMBER", menu);
  modals_module_show_banner("Exiting...");
}

static void get_reset_menu() {
  int stored = preferences_get_int("MENUNUMBER", MENU_MAIN);

  /* The id comes from NVS, which survives `idf.py flash` and board-profile
     switches, so it can name a menu that this build does not compile in.
     Resolving through menu_at() would then yield catalogue[0], whose
     parent_idx is -1: current_menu would end up as -1, update_menus() would
     find nothing, and navigation_exit() could never get back to MENU_MAIN.
     Reject anything that does not resolve and start from the root. */
  if (menus_find((menu_idx_t) stored) == NULL) {
    if (stored != MENU_MAIN) {
      ESP_LOGW(TAG, "MENUNUMBER %d is not compiled in, starting at the main menu",
               stored);
    }
    stored = MENU_MAIN;
  }

  menus_ctx->current_menu = (menu_idx_t) stored;
  menus_ctx->parent_menu_idx = menu_at(menus_ctx->current_menu)->parent_idx;
  update_menus();

  if (stored == MENU_MAIN) {
    show_logo();
    return;
  }
  /* Resuming into an app after a restart: only the main menu is persistent. */
  preferences_put_int("MENUNUMBER", MENU_MAIN);
  sleep_mode_reset_timer();
  screen_saver_get_idle_state();
  refresh_menus();
}

void menus_module_enable_input() {
  menus_ctx->input_lock = false;
}
void menus_module_disable_input() {
  menus_ctx->input_lock = true;
}

void menus_module_set_app_state(bool in_app, input_callback_t input_cb) {
  app_state.in_app = in_app;
  app_state.input_last_callback = app_state.input_callback;
  app_state.input_callback = input_cb;
  sleep_mode_reset_timer();
  screen_saver_get_idle_state();
}

void menus_module_set_app_state_last() {
  app_state.input_callback = app_state.input_last_callback;
}

void menus_module_set_default_input() {
  app_state.in_app = false;
  app_state.input_callback = menus_input_cb;
}

void menus_module_return() {
  menus_module_set_default_input();
  menus_ctx->current_menu = menus_ctx->parent_menu_idx;
  menus_ctx->parent_menu_idx =
      menu_at(menus_ctx->current_menu)->parent_idx;
  refresh_menus();
}

void menus_module_reset() {
  preferences_put_int("MENUNUMBER", menus_ctx->parent_menu_idx);
  esp_restart();
}

void menus_module_exit_app() {
  menus_module_set_app_state(false, menus_input_cb);
  sleep_mode_reset_timer();
  screen_saver_get_idle_state();
  navigation_exit();
}

menu_idx_t menus_module_get_current_menu() {
  return menus_ctx->current_menu;
}

bool menus_module_get_app_state() {
  return app_state.in_app;
}

void menus_module_set_menu(menu_idx_t menu_idx) {
  menu_t* target = menus_find(menu_idx);
  if (!target) {
    ESP_LOGE(TAG, "Menu %d is not compiled in", menu_idx);
    return;
  }
  screen_saver_stop();
  keyboard_module_set_input_callback(menus_input_cb);
  menu_at(menus_ctx->current_menu)->last_selected_submenu =
      menus_ctx->selected_submenu;
  menus_ctx->current_menu = target->menu_idx;
  menus_ctx->parent_menu_idx = menu_at(menus_ctx->current_menu)->parent_idx;
  refresh_menus();
  menu_cb_t cb = menu_at(menus_ctx->current_menu)->on_enter_cb;
  if (cb) {
    cb();
  }
}

void menus_module_set_menu_over_cmd(const char* entry_cmd) {
  int idx = get_menu_idx_over_cmd(entry_cmd);
  if (idx < 0) {
    return;  /* get_menu_idx_over_cmd() already logged the reason */
  }
  menus_module_set_menu(catalogue[idx].menu_idx);
}

void menus_module_hide_menu(menu_idx_t menu_idx) {
  menu_t* menu = menu_at_checked(menu_idx);
  if (menu) {
    menu->is_visible = false;
  }
}

void menus_module_reveal_menu(menu_idx_t menu_idx) {
  menu_t* menu = menu_at_checked(menu_idx);
  if (menu) {
    menu->is_visible = true;
  }
}
void menus_module_refresh() {
  refresh_menus();
}

void menus_module_begin() {
  catalogue = menus_get();
  menus_ctx = calloc(1, sizeof(menus_manager_t));
  if (menus_ctx == NULL) {
    ESP_LOGE(TAG, "Out of memory allocating the menu context");
    return;
  }
  menus_ctx->menus_count = menus_count();
  screen_saver_begin();
  // sleep_mode_begin();
  keyboard_module_set_input_callback(menus_input_cb);
  oled_screen_begin();
  get_reset_menu();
  update_menus();
}