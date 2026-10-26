/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gatt_dispatcher.h
 *
 * Central fan-out dispatcher for Bluedroid BLE GATT-client events.
 *
 * Bluedroid keeps a single GATTC callback slot
 * (esp_ble_gattc_register_callback) and a global app_id table
 * (esp_ble_gattc_app_register). Two modules that both call
 * esp_ble_gattc_register_callback() pick the same slot, so the last
 * registration silently steals every event from the previous one, and the
 * unregistered module stops receiving GATTC events at all.
 *
 * This is the same failure mode that gap_dispatcher solves for GAP events.
 * gatt_dispatcher_register() installs the module's handler into a fan-out
 * table; every GATTC event is then delivered to all of them. Handlers must
 * filter events by gattc_if (the examples in this tree already do), so an
 * event aimed at another module's interface is ignored naturally.
 */
#pragma once

#include "sdkconfig.h"

#if CONFIG_BT_ENABLED
  #include "esp_err.h"
  #include "esp_gattc_api.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install the dispatcher into Bluedroid once (idempotent).
 *
 * Call esp_ble_gattc_register_callback(gatt_dispatcher_dispatch) the first
 * time a subscriber registers. Bluedroid takes the dispatcher down only when
 * the stack itself (Bluedroid) is disabled, which radio_lease_ble_release()
 * does; gatt_dispatcher_reset() marks the slot as gone so the next
 * registration reinstalls it.
 *
 * @return ESP_OK on success or the error from
 * esp_ble_gattc_register_callback().
 */
esp_err_t gatt_dispatcher_init(void);

/**
 * @brief Register a GATT-client event handler to receive all GATTC events.
 *
 * Duplicate registrations of the same callback are ignored.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if cb is NULL,
 *         ESP_ERR_NO_MEM if the subscriber list is full.
 */
esp_err_t gatt_dispatcher_register(esp_gattc_cb_t cb);

/**
 * @brief Unregister a previously registered GATTC handler.
 *
 * When the last subscriber leaves, the dispatcher's own (possibly stale)
 * registration into Bluedroid no longer matters for correctness.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if cb is NULL,
 *         ESP_ERR_NOT_FOUND if the callback was not registered.
 */
esp_err_t gatt_dispatcher_unregister(esp_gattc_cb_t cb);

/**
 * @brief Forget that the dispatcher is registered into Bluedroid.
 *
 * Call it whenever the BLE stack is disabled or deinitialised (radio_lease
 * teardown), so the next gatt_dispatcher_register() reinstalls the internal
 * callback into a freshly enabled Bluedroid.
 *
 * @return Always ESP_OK.
 */
esp_err_t gatt_dispatcher_reset(void);

#ifdef __cplusplus
}
#endif
