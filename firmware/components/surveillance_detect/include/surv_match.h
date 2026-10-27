// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "surv_signatures.h"
#include "surv_types.h"

// Builds the prefilter. Call once before any match.
void surv_match_init(void);

// Rebuilds the prefilter bitmap after a change to the effective OUI table
// (overlay). Must be called after surv_signatures_build_effective().
void surv_match_rebuild_bitmap(void);

// Returns the table entry or NULL. Safe to call from the promiscuous callback:
// no memory allocation, no I/O.
const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6]);

// Case-insensitive substring search. Handles NULL and empty needle.
bool surv_match_contains_ci(const char* hay, const char* needle);

// Looks up SSID in the keyword table. Returns the first match or NULL.
const surv_kw_entry_t* surv_match_ssid(const char* ssid);

#define SURV_BLE_MAX_HITS 4
typedef struct {
  surv_class_t klass;
  uint8_t points;
  const char* label;  // "AirTag", "Axon", ... static string
} surv_ble_hit_t;

// Walks the TLVs of a BLE advertisement and accumulates up to SURV_BLE_MAX_HITS
// distinct classes (a single advertisement can be an iBeacon and something else
// at the same time). adv may come straight from over the air: adv_len is the
// actual buffer length, untrusted on its own. Returns the number of hits in
// out.
uint8_t surv_match_ble_adv(const uint8_t* adv,
                           uint8_t adv_len,
                           surv_ble_hit_t out[SURV_BLE_MAX_HITS]);
