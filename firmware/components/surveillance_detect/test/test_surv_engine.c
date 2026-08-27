// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_engine.h"
#include "surv_test.h"

static int s_emits;
static uint8_t s_last_score;

static void on_emit(const surv_event_t* ev, uint8_t score) {
  (void) ev;
  s_emits++;
  s_last_score = score;
}

static surv_event_t mk(uint8_t last_octet, surv_class_t k, uint8_t tier) {
  surv_event_t ev = {0};
  ev.mac[0] = 0x70;
  ev.mac[1] = 0xc9;
  ev.mac[2] = 0x4e;
  ev.mac[5] = last_octet;
  ev.klass = k;
  ev.tier = tier;
  ev.rssi = -50;
  ev.proto = SURV_PROTO_WIFI;
  return ev;
}

TEST_CASE("una deteccion suma sus puntos al score", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
}

TEST_CASE("el score decae 1 punto cada 60 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_tick(1000 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
  surv_engine_tick(1000 + 120000);
  TEST_ASSERT_EQUAL_UINT8(3, surv_engine_score());
}

TEST_CASE("el score nunca baja de cero", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_tick(600000);
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("la misma clase no re-puntua antes de 120 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t a = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t b = mk(0x02, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&a, 5, 1000);
  surv_engine_submit(&b, 5, 30000);  // otra MAC, misma clase, dentro de 120 s
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  surv_engine_submit(&b, 5, 1000 + 120001);
  TEST_ASSERT_EQUAL_UINT8(10, surv_engine_score());
}

TEST_CASE("un tier alto no se degrada con uno bajo", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 2000);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_IE_SIG, surv_engine_best_tier(hi.mac));
}

TEST_CASE("el dedupe de 5 s suprime repeticiones del mismo tier",
          "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR2);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_submit(&ev, 5, 3000);
  TEST_ASSERT_EQUAL_INT(1, s_emits);
  surv_engine_submit(&ev, 5, 6001);
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("un tier superior preempta el dedupe", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&lo, 5, 1000);
  surv_engine_submit(&hi, 5, 1500);  // dentro de los 5 s, pero tier mayor
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("una MAC desconocida vista 3 veces en 5 min puntua persistencia",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x01};
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 1000));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 120000));
  // tercera vista, y han pasado mas de 5 min desde la primera
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac, 1000 + 300001));
  TEST_ASSERT_EQUAL_UINT8(2, surv_engine_score());
}

TEST_CASE("una MAC desconocida vista 3 veces en 1 min no puntua",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x02};
  surv_engine_note_unknown(mac, 1000);
  surv_engine_note_unknown(mac, 20000);
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 40000));
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE(
    "una MAC rastreada se purga tras 30 min de ausencia y el slot se reusa",
    "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac_a[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x03};
  const uint8_t mac_b[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x04};
  surv_engine_note_unknown(mac_a, 1000);
  // mac_a no vuelve a verse; pasan mas de 30 min y otra MAC entra: el scan
  // de purga debe liberar el slot de mac_a y reusarlo para mac_b.
  surv_engine_note_unknown(mac_b, 1000 + 1800001);
  // Si el slot de mac_a no se hubiese purgado, su conteo de "seen" seguiria
  // en pie y dispararia persistencia en la SEGUNDA vista de aqui en mas
  // (porque el span de 5 min desde el first_ms original de hace 30 min ya se
  // cumple). En cambio, al haberse reusado el slot, mac_a arranca de cero:
  // hacen falta 3 vistas nuevas antes de puntuar.
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac_a, 1000 + 1800002));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac_a, 1000 + 1800002 + 1));
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac_a, 1000 + 1800002 + 300001));
}

TEST_CASE("un tier inferior tras uno superior no fuerza re-emision",
          "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 1500);  // tier menor, dentro de los 5 s de dedupe
  TEST_ASSERT_EQUAL_INT(1, s_emits);
}

TEST_CASE(
    "el ancla de decay establecida por tick sobrevive a la primera "
    "deteccion",
    "[surv][engine]") {
  surv_engine_reset();
  // Ticks previos a cualquier deteccion, con score en 0: simulan el loop de
  // ~100 ms del engine task antes de que aparezca la primera senal. Cada uno
  // debe mover el ancla de decay (s_last_decay_ms), tal como hacia el codigo
  // original basado en el sentinela ms==0.
  surv_engine_tick(100);
  surv_engine_tick(200);
  surv_engine_tick(300);
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  // El decay debe dispararse 60 s despues del ultimo ancla establecida por
  // tick (300), NO 60 s despues del now_ms del primer submit (1000). Si el
  // primer submit hubiese pisado el ancla con su propio now_ms, este tick a
  // 300+60000 todavia no alcanzaria el intervalo de decay y el score
  // seguiria en 5.
  surv_engine_tick(300 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
}

TEST_CASE("la tabla se satura a 200 MAC sin desbordar", "[surv][engine]") {
  surv_engine_reset();
  for (int i = 0; i < 300; i++) {
    surv_event_t ev = mk((uint8_t) (i & 0xff), SURV_CLASS_CAM, SURV_TIER_ADDR2);
    ev.mac[4] = (uint8_t) (i >> 8);
    surv_engine_submit(&ev, 3, (uint32_t) (1000 + i * 10));
  }
  TEST_PASS();  // no debe crashear
}
