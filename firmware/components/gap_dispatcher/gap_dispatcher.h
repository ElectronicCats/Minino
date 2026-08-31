/*
 * gap_dispatcher.h
 *
 * Central fan-out dispatcher for Bluedroid BLE GAP events.
 *
 * Bluedroid only supports a single GAP callback registered via
 * esp_ble_gap_register_callback(). If multiple modules call that API, each
 * call overwrites the previous one and prior modules silently miss events
 * (which is a common cause of "BLE scanning/ADV stopped working").
 *
 * This component registers itself ONCE as the single GAP callback and
 * forwards every event to all subscribed handlers. Modules that need GAP
 * events must register/unregister via gap_dispatcher_register() /
 * gap_dispatcher_unregister() instead of calling
 * esp_ble_gap_register_callback() directly.
 */
#pragma once

#include "esp_err.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ensure the dispatcher is initialized (idempotent).
 *
 * Registers the internal dispatch callback as the single GAP callback if it
 * has not been registered yet. This is called automatically by
 * gap_dispatcher_register(), so it only needs to be called explicitly if you
 * want to guarantee registration earlier.
 *
 * @return ESP_OK on success, or an error code from
 * esp_ble_gap_register_callback().
 */
esp_err_t gap_dispatcher_init(void);

/**
 * @brief Register a GAP event handler to receive all GAP events.
 *
 * Safe to call multiple times for the same callback; duplicates are ignored.
 *
 * @param cb GAP callback to subscribe.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if cb is NULL,
 *         ESP_ERR_NO_MEM if the subscriber list is full.
 */
esp_err_t gap_dispatcher_register(esp_gap_ble_cb_t cb);

/**
 * @brief Unregister a previously registered GAP event handler.
 *
 * @param cb GAP callback to unsubscribe.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if cb is NULL,
 *         ESP_ERR_NOT_FOUND if the callback was not registered.
 */
esp_err_t gap_dispatcher_unregister(esp_gap_ble_cb_t cb);

#ifdef __cplusplus
}
#endif
