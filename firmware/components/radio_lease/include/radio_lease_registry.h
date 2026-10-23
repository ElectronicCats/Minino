/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Bookkeeping for shared-radio ownership, with no ESP-IDF dependencies so the
 * host tests can exercise it.
 *
 * Why
 * ---
 * The ESP32-C6 has one 2.4 GHz radio and one WiFi promiscuous callback slot.
 * Six subsystems wanted it: the WiFi analyzer/sniffer, the deauth detector, the
 * DroneID scanner, the RF spectrum analyzer, the SDR receiver and the
 * surveillance detector. They each called
 * esp_wifi_set_promiscuous_rx_cb()/esp_wifi_set_promiscuous() directly, so the
 * last one to start silently took the slot away from the others and
 * esp_wifi_set_promiscuous(false) at the end of a window killed a capture
 * somebody else was relying on.
 *
 * This registry keeps one entry per consumer and only touches the driver when
 * the first consumer arrives or the last one leaves.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Indexed by radio_prom_user_t; the glue layer supplies the enum size. */
#define RADIO_LEASE_MAX_USERS 8

typedef void (*radio_lease_rx_fn)(void* ctx, const void* buf, int type);

typedef struct {
  radio_lease_rx_fn fn[RADIO_LEASE_MAX_USERS];
  void* ctx[RADIO_LEASE_MAX_USERS];
  bool active[RADIO_LEASE_MAX_USERS];
  uint8_t count;
} radio_lease_registry_t;

/**
 * @brief Reset to "nobody holds it"
 */
void radio_lease_registry_reset(radio_lease_registry_t* reg);

/**
 * @brief Claim the radio for `user`, installing `fn` as its receiver
 *
 * The consumer id *is* its slot, so a full table cannot be overflowed and a
 * re-acquire only updates the receiver.
 *
 * @return ESP_ERR_INVALID_ARG for an out-of-range id or a NULL receiver
 */
esp_err_t radio_lease_registry_acquire(radio_lease_registry_t* reg,
                                       uint8_t user,
                                       radio_lease_rx_fn fn,
                                       void* ctx);

/**
 * @brief Swap the receiver of a consumer that already holds the radio
 *
 * @return ESP_ERR_NOT_FOUND if `user` does not hold it
 */
esp_err_t radio_lease_registry_set_cb(radio_lease_registry_t* reg,
                                      uint8_t user,
                                      radio_lease_rx_fn fn,
                                      void* ctx);

/**
 * @brief Give up the radio
 *
 * @return ESP_ERR_NOT_FOUND if `user` did not hold it
 */
esp_err_t radio_lease_registry_release(radio_lease_registry_t* reg,
                                       uint8_t user);

/**
 * @brief How many consumers currently hold the radio
 */
uint8_t radio_lease_registry_count(const radio_lease_registry_t* reg);

/**
 * @brief True when `user` currently holds the radio
 */
bool radio_lease_registry_holds(const radio_lease_registry_t* reg,
                                uint8_t user);

/**
 * @brief Deliver a frame to every active consumer
 *
 * Copies the consumer list before calling out, so a consumer that acquires or
 * releases from inside its own callback cannot corrupt the walk.
 *
 * @return the number of consumers invoked
 */
uint8_t radio_lease_registry_dispatch(radio_lease_registry_t* reg,
                                      const void* buf,
                                      int type);

/* --- plain reference counters, for the WiFi driver and the BLE stack ------ */

typedef enum {
  RADIO_STACK_WIFI = 0,
  RADIO_STACK_BLE,
} radio_stack_t;

/**
 * @brief Can `requested` be brought up with the current holders?
 *
 * WiFi and BLE are mutually exclusive on the ESP32-C6: their stacks do not fit
 * in internal SRAM together. A stack may only be acquired when the other one
 * has no holders; holders of the same stack share it.
 */
bool radio_lease_stack_allowed(uint8_t wifi_refs,
                               uint8_t ble_refs,
                               radio_stack_t requested);

/**
 * @brief Add one holder; returns the new count
 */
uint8_t radio_lease_ref_acquire(uint8_t* count);

/**
 * @brief Drop one holder; returns the new count
 */
uint8_t radio_lease_ref_release(uint8_t* count);

#ifdef __cplusplus
}
#endif
