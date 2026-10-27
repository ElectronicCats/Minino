// SPDX-License-Identifier: GPL-3.0-or-later
// Scoring and deduplication engine. Content: Task 7.
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
// Persistence tracker (SURV_CLASS_PERSIST, from eye-spy): an unknown MAC
// seen >=3 times across >=5 min is a potential follower.
// Returns true on the sighting that triggers scoring.
bool surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms);

// Traffic-light thresholds from the eye-spy scoring model (Apache-2.0):
// 0-2 CLEAR, 3-5 CAUTION, 6+ ALERT. Consumed by the screen and LEDs.
#define SURV_SCORE_CAUTION 3
#define SURV_SCORE_ALERT   6
