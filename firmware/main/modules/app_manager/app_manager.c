#include "app_manager.h"

#include "menus_module.h"

static const minino_app_t* s_active = NULL;

static void app_manager_input_cb(uint8_t button_name, uint8_t button_event) {
  const minino_app_t* app = s_active;
  if (app != NULL && app->on_input != NULL) {
    app->on_input(button_name, button_event);
  }
}

void app_manager_enter(const minino_app_t* app) {
  if (app == NULL || s_active != NULL) {
    return;
  }
  s_active = app;
  menus_module_set_app_state(true, app_manager_input_cb);
  if (app->on_enter != NULL) {
    app->on_enter();
  }
}

void app_manager_exit(void) {
  const minino_app_t* app = s_active;
  if (app == NULL) {
    return;
  }
  /* Clear first: on_exit may block, and input must not reach a half-stopped
     app while it does. */
  s_active = NULL;
  if (app->on_exit != NULL) {
    app->on_exit();
  }
  menus_module_return();
}

bool app_manager_is_active(void) {
  return s_active != NULL;
}

const minino_app_t* app_manager_active(void) {
  return s_active;
}

static uint32_t s_watchdog_timeout_ms = 0;
static bool s_watchdog_enabled = false;

void app_manager_watchdog_enable(uint32_t timeout_ms) {
  s_watchdog_timeout_ms = timeout_ms;
  s_watchdog_enabled = true;
}

void app_manager_watchdog_disable(void) {
  s_watchdog_enabled = false;
  s_watchdog_timeout_ms = 0;
}

void app_manager_feed_watchdog(void) {
  (void) s_watchdog_enabled;
  (void) s_watchdog_timeout_ms;
}
