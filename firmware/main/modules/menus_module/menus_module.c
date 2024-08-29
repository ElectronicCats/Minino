#include "menus_module.h"

#include <string.h>
#include "bitmaps_general.h"
#include "buzzer.h"
#include "led_events.h"
#include "menus_screens.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "screen_saver.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define SCREEN_WIDTH  128
  #define SCREEN_HEIGHT 64
#else  // CONFIG_RESOLUTION_128X32
  #define SCREEN_WIDTH  128
  #define SCREEN_HEIGHT 32
#endif

static menus_manager_t* menus_ctx;
static void menus_input_cb(uint8_t button_name, uint8_t button_event);
static app_state2_t app_state2 = {.in_app = false,
                                  .input_callback = NULL,
                                  .input_last_callback = NULL};
static TaskHandle_t screen_saver_task = NULL;
static bool screen_saver_running = false;

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
  menus_ctx->parent_menu_idx =
      menus[get_menu_idx(menus_ctx->current_menu)].parent_idx;
  refresh_menus();
  void (*cb)() = menus[get_menu_idx(menus_ctx->current_menu)].on_enter_cb;
  if (cb) {
    cb();
  }
}

static void navigation_exit() {
  if (menus_ctx->current_menu == MENU_MAIN_2) {
    screen_saver_run();
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
  menus_ctx->parent_menu_idx =
      menus[get_menu_idx(menus_ctx->current_menu)].parent_idx;
  refresh_menus();
}

static void menus_input_cb(uint8_t button_name, uint8_t button_event) {
  if (menus_ctx->input_lock) {
    return;
  }

  if (app_state2.in_app) {
    if (app_state2.input_callback) {
      app_state2.input_callback(button_name, button_event);
      return;
    }
    app_state2.in_app = false;
    return;
  }

  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  if (screen_saver_get_idle_state()) {
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
  menus_ctx->current_menu = preferences_get_int("MENUNUMBER", MENU_MAIN_2);
  if ((int) menus_ctx->current_menu == MENU_MAIN_2) {
    show_logo();
  } else {
    preferences_put_int("MENUNUMBER", MENU_MAIN_2);
    screen_saver_get_idle_state();
    refresh_menus();
  }
}

void menus_module_enable_input() {
  menus_ctx->input_lock = false;
}
void menus_module_disable_input() {
  menus_ctx->input_lock = true;
}

void menus_module_set_app_state(bool in_app, input_callback_t input_cb) {
  app_state2.in_app = in_app;
  app_state2.input_last_callback = app_state2.input_callback;
  app_state2.input_callback = input_cb;
  screen_saver_get_idle_state();
}

void menus_module_set_app_state_last() {
  app_state2.input_callback = app_state2.input_last_callback;
}

void menus_module_restart() {
  menus_module_set_reset_screen(menus_ctx->parent_menu_idx);
  esp_restart();
}

void menus_module_exit_app() {
  menus_module_set_app_state(false, menus_input_cb);
  screen_saver_get_idle_state();
  navigation_exit();
}

menu_idx_t menus_module_get_current_menu() {
  return menus_ctx->current_menu;
}

bool menus_module_get_app_state() {
  return app_state2.in_app;
}

void menus_module_begin() {
  menus_ctx = calloc(1, sizeof(menus_manager_t));
  menus_ctx->menus_count = sizeof(menus) / sizeof(menu_t);
  screen_saver_begin();
  keyboard_module_set_input_callback(menus_input_cb);
  oled_screen_begin();
  get_reset_menu();
  update_menus();
}