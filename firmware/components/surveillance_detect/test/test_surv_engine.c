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

TEST_CASE("a detection adds its points to the score", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
}

TEST_CASE("score decays by 1 point every 60 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_tick(1000 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
  surv_engine_tick(1000 + 120000);
  TEST_ASSERT_EQUAL_UINT8(3, surv_engine_score());
}

TEST_CASE("score never drops below zero", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_tick(600000);
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("same class does not re-score before 120 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t a = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t b = mk(0x02, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&a, 5, 1000);
  surv_engine_submit(&b, 5, 30000);  // another MAC, same class, within 120 s
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  surv_engine_submit(&b, 5, 1000 + 120001);
  TEST_ASSERT_EQUAL_UINT8(10, surv_engine_score());
}

TEST_CASE("high tier is not downgraded by a lower one", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 2000);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_IE_SIG, surv_engine_best_tier(hi.mac));
}

TEST_CASE("5 s dedupe suppresses repeats of the same tier", "[surv][engine]") {
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

TEST_CASE("higher tier preempts dedupe", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&lo, 5, 1000);
  surv_engine_submit(&hi, 5, 1500);  // within 5 s, but higher tier
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("unknown MAC seen 3 times over 5 min scores persistence",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x01};
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 1000));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 120000));
  // third sighting, and more than 5 min have elapsed since the first
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac, 1000 + 300001));
  TEST_ASSERT_EQUAL_UINT8(2, surv_engine_score());
}

TEST_CASE("unknown MAC seen 3 times in 1 min does not score",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x02};
  surv_engine_note_unknown(mac, 1000);
  surv_engine_note_unknown(mac, 20000);
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 40000));
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE(
    "tracked MAC is purged after 30 min of absence and its slot is reused",
    "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac_a[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x03};
  const uint8_t mac_b[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x04};
  surv_engine_note_unknown(mac_a, 1000);
  // mac_a is not seen again; more than 30 min pass and another MAC arrives:
  // the purge scan must free mac_a's slot and reuse it for mac_b.
  surv_engine_note_unknown(mac_b, 1000 + 1800001);
  // If mac_a's slot had not been purged, its "seen" count would still stand
  // and trigger persistence on the SECOND sighting from here on (since the
  // 5-min span from the original first_ms 30 min ago is already met).
  // Instead, because the slot was reused, mac_a starts from scratch: 3 new
  // sightings are needed before scoring.
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac_a, 1000 + 1800002));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac_a, 1000 + 1800002 + 1));
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac_a, 1000 + 1800002 + 300001));
}

TEST_CASE("lower tier after a higher one does not force re-emission",
          "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 1500);  // lower tier, within the 5 s dedupe window
  TEST_ASSERT_EQUAL_INT(1, s_emits);
}

TEST_CASE("decay anchor established by tick survives the first detection",
          "[surv][engine]") {
  surv_engine_reset();
  // Ticks prior to any detection, with score at 0: simulate the ~100 ms loop
  // of the engine task before the first signal appears. Each tick must advance
  // the decay anchor (s_last_decay_ms), matching the original ms==0 sentinel
  // behavior.
  surv_engine_tick(100);
  surv_engine_tick(200);
  surv_engine_tick(300);
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  // Decay must fire 60 s after the last anchor set by tick (300), NOT 60 s
  // after the first submit's now_ms (1000). If the first submit had
  // overwritten the anchor with its own now_ms, this tick at 300+60000 would
  // not yet reach the decay interval and the score would remain 5.
  surv_engine_tick(300 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
}

TEST_CASE("device table saturates at 200 MACs without overflowing",
          "[surv][engine]") {
  surv_engine_reset();
  for (int i = 0; i < 300; i++) {
    surv_event_t ev = mk((uint8_t) (i & 0xff), SURV_CLASS_CAM, SURV_TIER_ADDR2);
    ev.mac[4] = (uint8_t) (i >> 8);
    surv_engine_submit(&ev, 3, (uint32_t) (1000 + i * 10));
  }
  TEST_PASS();  // must not crash
}
