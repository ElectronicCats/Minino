// SPDX-License-Identifier: GPL-3.0-or-later
//
// WiFi promiscuous callback, lock-free queue to the scoring engine, and
// public surveillance detector API.
//
// Design discipline: the promiscuous callback runs in the WiFi driver task
// under real-time constraints. It matches and enqueues, nothing else --
// no printf, no malloc, no I/O, no blocking. Everything expensive (the
// scoring engine, deduplication, user callbacks) runs in the surv_engine
// task, which drains the queue.
#include "surveillance_detect.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radio_selector.h"
#include "surv_engine.h"
#include "surv_ie.h"
#include "surv_match.h"
#include "surv_radio.h"

#define SURV_QUEUE_LEN 32

// WiFi ODID engine points. Same weight as the ODID entry in the BLE table
// (surv_signatures.c); named so the duplication is visible.
#define SURV_PTS_ODID_WIFI 4

typedef struct {
  surv_event_t ev;
  uint8_t points;
} surv_qitem_t;

static surv_qitem_t s_queue[SURV_QUEUE_LEN];
static volatile uint8_t s_q_head, s_q_tail;
static volatile uint32_t s_overflows;
static portMUX_TYPE s_q_mux = portMUX_INITIALIZER_UNLOCKED;

bool surv_queue_push(const surv_event_t* ev, uint8_t points) {
  bool ok = false;
  if (xPortInIsrContext()) {
    portENTER_CRITICAL_ISR(&s_q_mux);
    uint8_t next = (uint8_t) ((s_q_head + 1) % SURV_QUEUE_LEN);
    if (next != s_q_tail) {
      s_queue[s_q_head].ev = *ev;
      s_queue[s_q_head].points = points;
      s_q_head = next;
      ok = true;
    } else {
      s_overflows++;
    }
    portEXIT_CRITICAL_ISR(&s_q_mux);
  } else {
    portENTER_CRITICAL(&s_q_mux);
    uint8_t next = (uint8_t) ((s_q_head + 1) % SURV_QUEUE_LEN);
    if (next != s_q_tail) {
      s_queue[s_q_head].ev = *ev;
      s_queue[s_q_head].points = points;
      s_q_head = next;
      ok = true;
    } else {
      s_overflows++;
    }
    portEXIT_CRITICAL(&s_q_mux);
  }
  return ok;
}

uint32_t surv_queue_overflows(void) {
  return s_overflows;
}

// The OUI entry `tier` is a CEILING, never the event tier itself. Written
// once: all three detection paths apply the same clamp, and getting the
// ternary backwards on any one of them would promote a contract manufacturer
// OUI to a confirmatory tier.
// surv_clamp_tier is defined in surv_types.h.

void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void) type;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*) buf;
  if (p->rx_ctrl.rssi < SURV_RSSI_MIN) {
    return;
  }
  // Drop frames flagged with a reception error by hardware, just like the
  // ESP-IDF sniffer example does. A corrupted frame can carry an altered
  // addr2 that happens to match a known OUI and feed garbage into the
  // tier-1/tier-2 paths (OUI-only, already the noisiest ones). The project
  // acceptance criterion is ZERO false positives for FLOCK/ALPR classes, so
  // this is not optional hygiene.
  if (p->rx_ctrl.rx_state != 0) {
    return;
  }
  const uint8_t* pl = p->payload;
  int len = p->rx_ctrl.sig_len;
  if (len < 24) {
    return;
  }
  const uint8_t* addr1 = pl + 4;
  const uint8_t* addr2 = pl + 10;
  const uint8_t* addr3 = pl + 16;
  uint8_t subtype = (uint8_t) ((pl[0] >> 4) & 0x0f);
  bool is_mgmt = ((pl[0] & 0x0c) == 0x00);

  surv_event_t ev = {0};
  ev.rssi = p->rx_ctrl.rssi;
  ev.channel = p->rx_ctrl.channel;
  ev.proto = SURV_PROTO_WIFI;

  // Tier 3/4: wildcard SSID probe request from a known OUI.
  if (is_mgmt && subtype == 4 && len > 24) {
    const surv_oui_entry_t* e = surv_match_oui(addr2);
    if (e != NULL) {
      const uint8_t* ies = pl + 24;
      int ielen = len - 24;
      if (surv_ie_is_wildcard_probe(ies, ielen) == 1) {
        memcpy(ev.mac, addr2, 6);
        ev.klass = e->klass;
        ev.tier =
            surv_clamp_tier(surv_ie_matches_flock(ies, ielen) ? SURV_TIER_IE_SIG
                                                              : SURV_TIER_PROBE,
                            e->tier);
        surv_queue_push(&ev, e->points);
        return;
      }
    }
  }

  // Drone Remote ID over WiFi: management frame addressed to the fixed ASTM
  // F3411 NaN MAC. Does not depend on any OUI. Engine from eye-spy
  // (Apache-2.0).
  static const uint8_t ODID_NAN[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
  if (is_mgmt && memcmp(addr1, ODID_NAN, 6) == 0) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = SURV_CLASS_ODID;
    ev.tier = SURV_TIER_ADDR2;
    surv_queue_push(&ev, SURV_PTS_ODID_WIFI);
    return;
  }

  // Tier 2: OUI in addr2 of any frame.
  const surv_oui_entry_t* e2 = surv_match_oui(addr2);
  if (e2 != NULL) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = e2->klass;
    ev.tier = surv_clamp_tier(SURV_TIER_ADDR2, e2->tier);
    surv_queue_push(&ev, e2->points);
    return;
  }

  // Tier 1: OUI in addr1 or addr3. Echoes, prone to false positives.
  const surv_oui_entry_t* e1 = surv_match_oui(addr1);
  if (e1 == NULL && is_mgmt) {
    e1 = surv_match_oui(addr3);
    addr1 = addr3;
  }
  if (e1 != NULL) {
    memcpy(ev.mac, addr1, 6);
    ev.klass = e1->klass;
    ev.tier = surv_clamp_tier(SURV_TIER_ADDR13, e1->tier);
    surv_queue_push(&ev, e1->points);
  }
}

static TaskHandle_t s_engine_task;
static volatile bool s_running;
static surv_detect_cb_t s_user_cb;

static void engine_emit(const surv_event_t* ev, uint8_t score) {
  if (s_user_cb != NULL) {
    s_user_cb(ev, score);
  }
}

static void engine_task(void* arg) {
  (void) arg;
  while (s_running) {
    uint32_t now = (uint32_t) (esp_timer_get_time() / 1000);
    while (s_q_tail != s_q_head) {
      surv_qitem_t item;
      portENTER_CRITICAL(&s_q_mux);
      item = s_queue[s_q_tail];
      s_q_tail = (uint8_t) ((s_q_tail + 1) % SURV_QUEUE_LEN);
      portEXIT_CRITICAL(&s_q_mux);
      surv_engine_submit(&item.ev, item.points, now);
    }
    surv_engine_tick(now);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  s_engine_task = NULL;
  vTaskDelete(NULL);
}

// Common steps for surv_begin() and surv_begin_phased() ("Scan All" mux):
// prepare the matcher and engine, WITHOUT starting the radio stack, which
// each variant decides on its own.
static esp_err_t surv_begin_common(surv_profile_t profile, bool active_scan) {
  if (s_running) {
    return ESP_ERR_INVALID_STATE;
  }
  // surv_match_init() must run exactly once before enabling the promiscuous
  // callback: without it surv_match_oui() always returns NULL and the
  // detector never fires, silently (no logging possible on the hot path).
  surv_match_init();
  surv_engine_reset();
  surv_engine_register_emit_cb(engine_emit);
  s_q_head = 0;
  s_q_tail = 0;
  s_overflows = 0;
  radio_selector_set_surveillance();
  s_running = true;
  if (xTaskCreate(engine_task, "surv_engine", 8192, NULL, 5, &s_engine_task) !=
      pdPASS) {
    s_running = false;
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

esp_err_t surv_begin(surv_profile_t profile, bool active_scan) {
  esp_err_t err = surv_begin_common(profile, active_scan);
  if (err != ESP_OK) {
    return err;
  }
  surv_radio_start(profile, active_scan);
  return err;
}

// "Scan All" mux variant: starts the common infrastructure and then a single
// radio window (BLE or WiFi) that flips phase and reboots when finished.
// Returns the error from surv_begin_common (start_once failure degrades to
// stop).
esp_err_t surv_begin_phased(surv_profile_t profile,
                            bool active_scan,
                            surv_radio_phase_t phase,
                            void (*on_phase_done)(void)) {
  esp_err_t err = surv_begin_common(profile, active_scan);
  if (err != ESP_OK) {
    return err;
  }
  esp_err_t rerr =
      surv_radio_start_once(profile, active_scan, phase, on_phase_done);
  if (rerr != ESP_OK) {
    surv_stop();
    return rerr;
  }
  return ESP_OK;
}

void surv_stop(void) {
  s_running = false;
  surv_radio_stop();
  // Wait for engine_task to finish and self-delete.
  while (s_engine_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void surv_register_cb(surv_detect_cb_t cb) {
  s_user_cb = cb;
}
