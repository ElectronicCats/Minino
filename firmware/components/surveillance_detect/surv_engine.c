// SPDX-License-Identifier: GPL-3.0-or-later
//
// Motor de confianza. Combina dos modelos:
//   - tier por MAC, de flock-you (MIT): que tan seguro estoy de este equipo.
//   - score global con decay, de eye-spy (Apache-2.0): que tan vigilado esta
//     este lugar.
// Son ejes independientes: un SSID "flock" es tier 0 y a la vez 5 puntos.
#include "surv_engine.h"
#include <string.h>

#define SURV_MAX_DEVICES 200
#define SURV_DECAY_MS    60000u
#define SURV_RESCORE_MS  120000u
#define SURV_DEDUPE_MS   5000u

typedef struct {
  uint8_t mac[6];
  uint8_t best_tier;
  uint32_t last_emit_ms;
  bool last_emit_set;  // false = todavia no emitio nada, distinto de ms==0
  bool used;
} surv_dev_t;

static surv_dev_t s_devs[SURV_MAX_DEVICES];
static uint16_t s_dev_count;
static uint32_t s_class_scored_ms[SURV_CLASS_MAX];
static bool s_class_seen[SURV_CLASS_MAX];
static uint8_t s_score;
static uint32_t s_last_decay_ms;
static bool s_decay_anchor_set;  // false = ancla de decay sin fijar aun
static surv_engine_emit_cb_t s_emit_cb;

// Rastreador de persistencia (de eye-spy, Apache-2.0): declarado aqui arriba
// para que surv_engine_reset() pueda limpiarlo junto con todo lo demas.
#define SURV_TRACK_MAX      50
#define SURV_TRACK_MIN_SEEN 3
#define SURV_TRACK_MIN_SPAN 300000u   // 5 min
#define SURV_TRACK_PURGE_MS 1800000u  // 30 min

typedef struct {
  uint8_t mac[6];
  uint8_t seen;
  uint32_t first_ms;
  uint32_t last_ms;
  bool scored;
  bool used;
} surv_track_t;

static surv_track_t s_track[SURV_TRACK_MAX];

void surv_engine_reset(void) {
  memset(s_devs, 0, sizeof(s_devs));
  memset(s_class_scored_ms, 0, sizeof(s_class_scored_ms));
  memset(s_class_seen, 0, sizeof(s_class_seen));
  s_dev_count = 0;
  s_score = 0;
  s_last_decay_ms = 0;
  s_decay_anchor_set = false;
  s_emit_cb = NULL;
  memset(s_track, 0, sizeof(s_track));
}

void surv_engine_register_emit_cb(surv_engine_emit_cb_t cb) {
  s_emit_cb = cb;
}

static surv_dev_t* find_or_add(const uint8_t mac[6]) {
  for (uint16_t i = 0; i < s_dev_count; i++) {
    if (memcmp(s_devs[i].mac, mac, 6) == 0) {
      return &s_devs[i];
    }
  }
  if (s_dev_count >= SURV_MAX_DEVICES) {
    return NULL;  // tabla llena: se sigue puntuando, no se rastrea el equipo
  }
  surv_dev_t* d = &s_devs[s_dev_count++];
  memcpy(d->mac, mac, 6);
  d->best_tier = 0;
  d->last_emit_ms = 0;
  d->last_emit_set = false;
  d->used = true;
  return d;
}

uint8_t surv_engine_best_tier(const uint8_t mac[6]) {
  for (uint16_t i = 0; i < s_dev_count; i++) {
    if (memcmp(s_devs[i].mac, mac, 6) == 0) {
      return s_devs[i].best_tier;
    }
  }
  return 0;
}

void surv_engine_submit(const surv_event_t* ev,
                        uint8_t points,
                        uint32_t now_ms) {
  if (ev == NULL || ev->klass <= SURV_CLASS_NONE ||
      ev->klass >= SURV_CLASS_MAX) {
    return;
  }
  if (ev->rssi < SURV_RSSI_MIN) {
    return;
  }

  surv_dev_t* d = find_or_add(ev->mac);
  bool tier_up = (d != NULL) && (ev->tier > d->best_tier);
  if (d != NULL && tier_up) {
    d->best_tier = ev->tier;
  }

  // Score global: cooldown por clase, no por MAC.
  bool first = !s_class_seen[ev->klass];
  if (first || (now_ms - s_class_scored_ms[ev->klass]) > SURV_RESCORE_MS) {
    uint16_t sum = (uint16_t) s_score + points;
    s_score = (uint8_t) (sum > 255 ? 255 : sum);
    s_class_scored_ms[ev->klass] = now_ms;
    s_class_seen[ev->klass] = true;
    if (!s_decay_anchor_set) {
      s_last_decay_ms = now_ms;
      s_decay_anchor_set = true;
    }
  }

  // Emision: dedupe de 5 s por MAC, preemptado por un tier superior.
  bool emit = true;
  if (d != NULL) {
    if (!tier_up && d->last_emit_set &&
        (now_ms - d->last_emit_ms) < SURV_DEDUPE_MS) {
      emit = false;
    }
    if (emit) {
      d->last_emit_ms = now_ms;
      d->last_emit_set = true;
    }
  }
  if (emit && s_emit_cb != NULL) {
    s_emit_cb(ev, s_score);
  }
}

void surv_engine_tick(uint32_t now_ms) {
  if (s_score == 0) {
    s_last_decay_ms = now_ms;
    return;
  }
  while (s_score > 0 && (now_ms - s_last_decay_ms) >= SURV_DECAY_MS) {
    s_score--;
    s_last_decay_ms += SURV_DECAY_MS;
  }
}

uint8_t surv_engine_score(void) {
  return s_score;
}

// --- Rastreador de persistencia (de eye-spy, Apache-2.0) --------------------
// Una MAC que no matchea ninguna firma pero reaparece a lo largo del tiempo es
// la senal de un seguidor. Se purga a los 30 min de ausencia.

bool surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms) {
  surv_track_t* slot = NULL;
  for (int i = 0; i < SURV_TRACK_MAX; i++) {
    if (s_track[i].used &&
        (now_ms - s_track[i].last_ms) > SURV_TRACK_PURGE_MS) {
      s_track[i].used = false;
    }
    if (s_track[i].used && memcmp(s_track[i].mac, mac, 6) == 0) {
      slot = &s_track[i];
      break;
    }
    if (!s_track[i].used && slot == NULL) {
      slot = &s_track[i];
    }
  }
  if (slot == NULL) {
    return false;
  }
  if (!slot->used) {
    memcpy(slot->mac, mac, 6);
    slot->used = true;
    slot->seen = 0;
    slot->scored = false;
    slot->first_ms = now_ms;
  }
  slot->seen++;
  slot->last_ms = now_ms;
  if (!slot->scored && slot->seen >= SURV_TRACK_MIN_SEEN &&
      (now_ms - slot->first_ms) >= SURV_TRACK_MIN_SPAN) {
    slot->scored = true;
    surv_event_t ev = {0};
    memcpy(ev.mac, mac, 6);
    ev.klass = SURV_CLASS_PERSIST;
    ev.tier = SURV_TIER_SSID;
    ev.rssi = 0;
    ev.proto = SURV_PROTO_BLE;
    surv_engine_submit(&ev, 2, now_ms);
    return true;
  }
  return false;
}
