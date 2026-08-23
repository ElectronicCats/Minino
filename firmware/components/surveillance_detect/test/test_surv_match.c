// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_match.h"
#include "surv_signatures.h"
#include "surv_test.h"

TEST_CASE("un OUI de Flock matchea con clase FLOCK", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("una MAC desconocida no matchea", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("la MAC localmente administrada 82:6b:f2 SI matchea",
          "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x82, 0x6b, 0xf2, 0x14, 0x07, 0x3a};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("el OUI de fabricante contratista tiene techo tier 1",
          "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xf4, 0x6a, 0xdd, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK_MFR, e->klass);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_ADDR13, e->tier);
}

TEST_CASE("ningun OUI aparece en dos clases distintas", "[surv][match]") {
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      TEST_ASSERT_FALSE_MESSAGE(memcmp(t[i].oui, t[j].oui, 3) == 0,
                                "OUI duplicado en la tabla base");
    }
  }
}
