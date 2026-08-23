// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_ie.h"
#include "surv_match.h"
#include "surv_overlay.h"
#include "surv_test.h"

TEST_CASE("un OUI anadido pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  surv_match_init();
  const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("un OUI quitado deja de matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("-oui,70:c9:4e\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.removed);
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("los comentarios y las lineas vacias se ignoran", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("# comentario\n\n   \n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(0, st.removed);
  TEST_ASSERT_EQUAL_UINT16(0, st.skipped);
}

TEST_CASE("una linea malformada se cuenta como saltada", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(
      "+oui,noesunamac,flock,5,2\n"
      "+oui,aa:bb\n"
      "basura\n"
      "+oui,dd:ee:ff,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  TEST_ASSERT_EQUAL_UINT16(3, st.skipped);
}

TEST_CASE("un texto nulo o vacio no rompe", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(NULL);
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  st = surv_overlay_parse("");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
}

TEST_CASE("una keyword de SSID anadida pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+ssid,mikamara,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const surv_kw_entry_t* e = surv_match_ssid("red-MiKaMaRa-2");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
}

TEST_CASE("un UUID anadido pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+uuid,fd6f,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t adv[] = {0x03, 0x03, 0x6f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(1, surv_match_ble_adv(adv, sizeof(adv), hits));
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, hits[0].klass);
}

TEST_CASE("una firma IE del overlay se suma a la compilada",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,45,191\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0x02, 0x00,
                         0x00, 0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
  surv_overlay_reset();
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("una firma IE malformada se salta", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221:abc\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// vlen es vestigial: el matcher decide comparar bytes de vendor mirando solo
// tag==221 (SURV_IE_VENDOR_TAG), nunca vlen. Un token "221" pelado, sin los
// 14 hex del payload, se "parsearia" bien y despues jamas haria match con
// nada en silencio si no se rechaza aqui.
TEST_CASE("un token 221 sin payload de vendor se salta", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("se respeta el tope de 256 OUIs", "[surv][overlay]") {
  surv_overlay_reset();
  char buf[64];
  surv_overlay_stats_t st = {0};
  for (int i = 0; i < 300; i++) {
    snprintf(buf, sizeof(buf), "+oui,%02x:%02x:%02x,cam,3,2\n",
             (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
    surv_overlay_stats_t r = surv_overlay_parse(buf);
    st.added += r.added;
    st.skipped += r.skipped;
  }
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(256, st.added);
  TEST_ASSERT_GREATER_THAN_UINT16(0, st.skipped);
}
