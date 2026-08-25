// SPDX-License-Identifier: GPL-3.0-or-later
// Motor de scoring y deduplicacion. Contenido: Task 7.
#pragma once
#include "surv_types.h"

typedef void (*surv_engine_emit_cb_t)(const surv_event_t* ev, uint8_t score);
void surv_engine_reset(void);
void surv_engine_register_emit_cb(surv_engine_emit_cb_t cb);
void surv_engine_submit(const surv_event_t* ev,
                        uint8_t points,
                        uint32_t now_ms);
void surv_engine_tick(uint32_t now_ms);
uint8_t surv_engine_score(void);
uint8_t surv_engine_best_tier(const uint8_t mac[6]);
// Rastreador de persistencia (SURV_CLASS_PERSIST, de eye-spy): una MAC
// desconocida vista >=3 veces a lo largo de >=5 min es un posible seguidor.
// Devuelve true la vez que dispara la puntuacion.
bool surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms);

// Umbrales del semaforo, del modelo de score de eye-spy (Apache-2.0):
// 0-2 CLEAR, 3-5 CAUTION, 6+ ALERT. Los consumen la pantalla y los LEDs.
#define SURV_SCORE_CAUTION 3
#define SURV_SCORE_ALERT   6
