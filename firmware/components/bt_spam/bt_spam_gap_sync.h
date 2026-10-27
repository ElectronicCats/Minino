/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Event matching for the BLE spam advertising loop.
 *
 * Bluedroid reports four different completion events through one binary
 * semaphore (ADV_STOP, SET_STATIC_RAND_ADDR, ADV_DATA_RAW_SET, ADV_START).
 * Returning "done" for whichever token happens to wake the waiter
 * desynchronises the cycle: a single stale token shifts every later wait by one
 * step, so is_advertising_active no longer reflects the controller and the loop
 * issues stop/configure/start without confirmation.
 *
 * This module owns the "did this wake-up answer the question I asked?" decision
 * so it can be unit tested on the host. bt_spam.c wraps it in the FreeRTOS
 * semaphore and the timeout.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /* Monotonic count of GAP completions seen since the last reset. */
  uint32_t completed;
  /* Event of the most recent completion. */
  esp_gap_ble_cb_event_t last_event;
  /* Status carried by the most recent completion. */
  esp_bt_status_t last_status;
} bt_spam_gap_sync_t;

/**
 * @brief Put the tracker back into its initial state
 */
void bt_spam_gap_sync_reset(bt_spam_gap_sync_t* sync);

/**
 * @brief Record a GAP completion event (call from the GAP callback)
 *
 * @return true when this completion is newer than the last one consumed by a
 *         wait, i.e. when a waiter still has to absorb it.
 */
bool bt_spam_gap_sync_complete(bt_spam_gap_sync_t* sync,
                               esp_gap_ble_cb_event_t event,
                               esp_bt_status_t status);

/**
 * @brief Value to snapshot before issuing an asynchronous GAP command
 *
 * Take this *after* draining the semaphore, so that completions belonging to
 * earlier commands are not mistaken for the answer to this one.
 */
uint32_t bt_spam_gap_sync_mark(const bt_spam_gap_sync_t* sync);

/**
 * @brief True when at least one completion happened since `mark`
 */
bool bt_spam_gap_sync_has_new(const bt_spam_gap_sync_t* sync, uint32_t mark);

/**
 * @brief Does the newest completion answer the request for `wanted`?
 *
 * A completion of some other event must not satisfy the wait: the caller keeps
 * looping until its deadline instead of proceeding on a stale status.
 */
bool bt_spam_gap_sync_matches(const bt_spam_gap_sync_t* sync,
                              esp_gap_ble_cb_event_t wanted);

/**
 * @brief Did the command we just issued succeed?
 *
 * Bluedroid reports most failures asynchronously in the completion event, not
 * in the return value, so this is only meaningful after a matching completion.
 */
bool bt_spam_gap_sync_succeeded(const bt_spam_gap_sync_t* sync);

#ifdef __cplusplus
}
#endif