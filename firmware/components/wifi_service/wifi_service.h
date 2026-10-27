/* WiFi Service singleton: encapsulates esp_wifi_init() and provides a common API.
 *
 * Before: 13 modules called esp_wifi_init() directly, each with its own
 * sdkconfig (#ifdef CONFIG_WIFI_*_ENABLE). Result: conditional recompilation,
 * duplicated logic, and redundant initializations.
 *
 * After: a single entry point that initializes the stack and exposes a stable
 * handle for the rest of the firmware. State changes are notified via direct
 * callbacks in each app; there is no event bus.
 *
 * Thread-safety: initialization and state are protected with a dedicated
 * spinlock. Mode getters query the driver directly, so they do not become
 * stale when an app calls esp_wifi_set_mode().
 */

#pragma once

#include "esp_wifi.h"

/* Internal state, protected by s_wifi_mux. */
typedef struct {
  wifi_mode_t mode;
  wifi_interface_t iface;
  bool is_up;
  bool is_sta;
  bool is_ap;
  wifi_second_chan_t primary_chan;
  wifi_second_chan_t secondary_chan;
} wifi_service_state_t;

/* Optional gate consulted before esp_wifi_init(). It lets the radio policy
 * (radio_lease) veto a bring-up while BLE owns the PHY, without making this
 * component depend on radio_lease. */
typedef bool (*wifi_service_init_guard_t)(void);
void wifi_service_set_init_guard(wifi_service_init_guard_t guard);

/* Public API. */
bool wifi_service_init(wifi_mode_t mode);
bool wifi_service_deinit(void);

/* Changes the mode and updates the service state.
 * Does not initialize the driver: wifi_service_init() must have run first. */
esp_err_t wifi_service_set_mode(wifi_mode_t mode);

/* Queryable state. Mode and STA/AP flags are read from the driver. */
wifi_mode_t wifi_service_mode_get(void);
wifi_interface_t wifi_service_iface_get(void);
bool wifi_service_is_up_get(void);
bool wifi_service_is_sta_get(void);
bool wifi_service_is_ap_get(void);
wifi_second_chan_t wifi_service_primary_chan_get(void);
wifi_second_chan_t wifi_service_secondary_chan_get(void);

/* --- Convenience helpers (optional) --- */

#define wifi_service_is_any_active() \
  (wifi_service_is_sta_get() || wifi_service_is_ap_get())
