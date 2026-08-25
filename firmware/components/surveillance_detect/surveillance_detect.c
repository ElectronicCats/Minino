// SPDX-License-Identifier: GPL-3.0-or-later
//
// Callback promiscuo de WiFi, cola sin bloqueo hacia el motor de scoring y
// API publica del detector de vigilancia.
//
// Disciplina de diseno: el callback promiscuo corre en la tarea del driver
// de WiFi, con restricciones de tiempo real. Compara y encola, nada mas --
// sin printf, sin malloc, sin E/S, sin bloqueo. Todo lo caro (el motor de
// scoring, la deduplicacion, los callbacks del usuario) pasa a la tarea
// surv_engine, que drena la cola.
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

// Puntos del motor ODID por WiFi. Mismo peso que la entrada ODID de la tabla
// BLE (surv_signatures.c); nombrado para que la duplicacion se vea.
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

// El `tier` de la entrada de OUI es un TECHO, nunca el tier del evento. Se
// escribe una sola vez: las tres rutas de deteccion aplican el mismo recorte
// y una de ellas con el ternario al reves promocionaria un OUI de fabricante
// contratista a tier confirmatorio.
// surv_clamp_tier se define en surv_types.h como surv_surv_clamp_tier().

void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void) type;
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*) buf;
  if (p->rx_ctrl.rssi < SURV_RSSI_MIN) {
    return;
  }
  // Descartar tramas que el hardware marco con error de recepcion, como hace
  // el sniffer de ejemplo de ESP-IDF. Una trama corrupta puede traer un addr2
  // alterado que coincida por azar con un OUI conocido, y alimentaria las
  // rutas tier-1/tier-2 -las de solo OUI, ya de por si las mas ruidosas- con
  // basura. El criterio de aceptacion del proyecto es CERO falsos positivos
  // de clase FLOCK/ALPR, asi que esto no es higiene opcional.
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

  // Tier 3/4: probe request con SSID wildcard desde un OUI conocido.
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

  // Drone Remote ID por WiFi: trama de gestion dirigida a la MAC NaN fija de
  // ASTM F3411. No depende de ningun OUI. Motor de eye-spy (Apache-2.0).
  static const uint8_t ODID_NAN[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
  if (is_mgmt && memcmp(addr1, ODID_NAN, 6) == 0) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = SURV_CLASS_ODID;
    ev.tier = SURV_TIER_ADDR2;
    surv_queue_push(&ev, SURV_PTS_ODID_WIFI);
    return;
  }

  // Tier 2: OUI en addr2 de cualquier trama.
  const surv_oui_entry_t* e2 = surv_match_oui(addr2);
  if (e2 != NULL) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = e2->klass;
    ev.tier = surv_clamp_tier(SURV_TIER_ADDR2, e2->tier);
    surv_queue_push(&ev, e2->points);
    return;
  }

  // Tier 1: OUI en addr1 o addr3. Ecos, propensos a falso positivo.
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

esp_err_t surv_begin(surv_profile_t profile, bool active_scan) {
  if (s_running) {
    return ESP_ERR_INVALID_STATE;
  }
  // surv_match_init() debe correr exactamente una vez antes de habilitar el
  // callback promiscuo: sin ella surv_match_oui() devuelve NULL siempre y el
  // detector no dispara jamas, en silencio (sin log posible en el hot path).
  surv_match_init();
  surv_engine_reset();
  surv_engine_register_emit_cb(engine_emit);
  s_q_head = 0;
  s_q_tail = 0;
  s_overflows = 0;
  radio_selector_set_surveillance();
  s_running = true;
  if (xTaskCreate(engine_task, "surv_engine", 4096, NULL, 5, &s_engine_task) !=
      pdPASS) {
    s_running = false;
    return ESP_ERR_NO_MEM;
  }
  esp_err_t err = surv_radio_start(profile, active_scan);
  if (err != ESP_OK) {
    surv_stop();
  }
  return err;
}

void surv_stop(void) {
  s_running = false;
  surv_radio_stop();
  // Esperar a que la tarea engine_task termine y se auto-elimine.
  while (s_engine_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void surv_register_cb(surv_detect_cb_t cb) {
  s_user_cb = cb;
}
