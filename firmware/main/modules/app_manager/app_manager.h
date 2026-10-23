#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Contract for an app that owns the screen and the input while the user is
 * inside it. The menu calls app_manager_enter() from on_enter_cb; the app
 * calls app_manager_exit() when it wants to go back.
 *
 * on_enter runs after the manager has installed itself as the input callback,
 * so an app may start tasks or draw immediately. on_exit runs exactly once,
 * before the menu is restored, and must stop every task and release every
 * radio lease the app acquired.
 */
typedef struct {
  const char* name;
  void (*on_enter)(void);
  void (*on_input)(uint8_t button_name, uint8_t button_event);
  void (*on_exit)(void);
} minino_app_t;

void app_manager_enter(const minino_app_t* app);
void app_manager_exit(void);
bool app_manager_is_active(void);
const minino_app_t* app_manager_active(void);

/* Watchdog API: call app_manager_feed_watchdog() periodically from the app's
 * main loop. If the watchdog expires, the system will reset.
 * Must be enabled before calling app_manager_enter() and disabled after exit.
 */
void app_manager_watchdog_enable(uint32_t timeout_ms);
void app_manager_watchdog_disable(void);
void app_manager_feed_watchdog(void);
