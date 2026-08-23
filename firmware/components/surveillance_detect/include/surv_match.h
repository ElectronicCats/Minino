// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "surv_signatures.h"
#include "surv_types.h"

// Construye el prefiltro. Llamar una vez antes de cualquier match.
void surv_match_init(void);

// Devuelve la entrada de la tabla o NULL. Apto para llamarse desde el callback
// promiscuo: sin asignación de memoria, sin I/O.
const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6]);

// Búsqueda case-insensitive de substring. Maneja NULL y empty needle.
bool surv_match_contains_ci(const char* hay, const char* needle);

// Busca SSID en la tabla de keywords. Devuelve la primera coincidencia o NULL.
const surv_kw_entry_t* surv_match_ssid(const char* ssid);

#define SURV_BLE_MAX_HITS 4
typedef struct {
  surv_class_t klass;
  uint8_t points;
  const char* label;  // "AirTag", "Axon", ... string estático
} surv_ble_hit_t;

// Recorre los TLV de un advertisement BLE y acumula hasta SURV_BLE_MAX_HITS
// clases distintas (un mismo advertisement puede ser iBeacon y otra cosa a la
// vez). adv puede venir directo del aire: adv_len es la longitud real del
// buffer, no confiable por sí sola. Devuelve el número de hits en out.
uint8_t surv_match_ble_adv(const uint8_t* adv,
                           uint8_t adv_len,
                           surv_ble_hit_t out[SURV_BLE_MAX_HITS]);
