// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tablas de firmas. Procedencia:
//   - OUIs de Flock: @NitekryDPaul (nite-oui-collection) vía flock-you (MIT)
//     y DeFlockJoplin (82:6b:f2).
//   - OUIs de cámaras, ALPR, SoundThinking y firmas BLE: eye-spy (Apache-2.0).
#pragma once
#include <stdint.h>
#include "surv_types.h"

typedef struct {
  uint8_t oui[3];
  surv_class_t klass;
  uint8_t points;  // peso en el score global
  uint8_t tier;    // TECHO de confianza para esta firma, no el tier del evento
} surv_oui_entry_t;

typedef struct {
  const char* kw;
  surv_class_t klass;
  uint8_t points;
} surv_kw_entry_t;

typedef struct {
  uint16_t uuid;  // service UUID de 16 bits
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

// Reconstruye la tabla efectiva = base + anadidos - quitados. La llama el
// overlay (Task 8). Con extra_count y removed_count en 0 devuelve la base.
void surv_signatures_build_effective(const surv_oui_entry_t* extra,
                                     uint16_t extra_count,
                                     const uint8_t (*removed)[3],
                                     uint16_t removed_count);
void surv_signatures_build_effective_kws(const surv_kw_entry_t* extra,
                                         uint16_t extra_count);
void surv_signatures_build_effective_uuids(const surv_uuid_entry_t* extra,
                                           uint16_t extra_count);
