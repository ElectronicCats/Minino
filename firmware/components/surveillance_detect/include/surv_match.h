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