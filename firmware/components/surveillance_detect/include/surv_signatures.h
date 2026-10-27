// SPDX-License-Identifier: GPL-3.0-or-later
//
// Signature tables. Provenance:
//   - Flock OUIs: @NitekryDPaul (nite-oui-collection) via flock-you (MIT)
//     and DeFlockJoplin (82:6b:f2).
//   - Camera, ALPR, SoundThinking OUIs and BLE signatures: eye-spy
//   (Apache-2.0).
#pragma once
#include <stdint.h>
#include "surv_types.h"

typedef struct {
  uint8_t oui[3];
  surv_class_t klass;
  uint8_t points;  // weight in global score
  uint8_t tier;    // confidence CEILING for this signature, not the event tier
} surv_oui_entry_t;

typedef struct {
  const char* kw;
  surv_class_t klass;
  uint8_t points;
} surv_kw_entry_t;

typedef struct {
  uint16_t uuid;  // 16-bit service UUID
  surv_class_t klass;
  uint8_t points;
  const char* label;
} surv_uuid_entry_t;

typedef struct {
  const char* name;  // substring, case-insensitive
  surv_class_t klass;
  uint8_t points;
  const char* label;
} surv_name_entry_t;

// Overlay caps. Single source of truth: here they size the effective arrays
// (surv_signatures.c) and in surv_overlay.c they limit how many entries are
// accepted. Defining them separately in each file, as originally done, leaves
// synchronization up to convention: if someone raises one without raising the
// other, surv_signatures_build_effective() writes past the end of a static
// array with no compiler diagnostic.
#define SURV_OVERLAY_MAX_OUIS  64
#define SURV_OVERLAY_MAX_KWS   16
#define SURV_OVERLAY_MAX_UUIDS 8

uint16_t surv_signatures_oui_count(void);
const surv_oui_entry_t* surv_signatures_ouis(void);
uint16_t surv_signatures_kw_count(void);
const surv_kw_entry_t* surv_signatures_kws(void);
uint16_t surv_signatures_uuid_count(void);
const surv_uuid_entry_t* surv_signatures_uuids(void);
uint16_t surv_signatures_ble_name_count(void);
const surv_name_entry_t* surv_signatures_ble_names(void);
uint16_t surv_signatures_skimmer_count(void);
const char* const* surv_signatures_skimmers(void);

// Rebuilds the effective table = base + additions - removals. Called by the
// overlay (Task 8). With extra_count and removed_count at 0, restores the base.
void surv_signatures_build_effective(const surv_oui_entry_t* extra,
                                     uint16_t extra_count,
                                     const uint8_t (*removed)[3],
                                     uint16_t removed_count);
void surv_signatures_build_effective_kws(const surv_kw_entry_t* extra,
                                         uint16_t extra_count);
void surv_signatures_build_effective_uuids(const surv_uuid_entry_t* extra,
                                           uint16_t extra_count);
