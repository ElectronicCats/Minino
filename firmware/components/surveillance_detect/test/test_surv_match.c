// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_match.h"
#include "surv_signatures.h"
#include "surv_test.h"

TEST_CASE("Flock OUI matches with class FLOCK", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("unknown MAC does not match", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("locally administered MAC 82:6b:f2 DOES match",
          "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x82, 0x6b, 0xf2, 0x14, 0x07, 0x3a};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("contract manufacturer OUI has tier ceiling 1",
          "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xf4, 0x6a, 0xdd, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK_MFR, e->klass);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_ADDR13, e->tier);
}

TEST_CASE("no OUI appears in two distinct classes", "[surv][match]") {
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      TEST_ASSERT_FALSE_MESSAGE(memcmp(t[i].oui, t[j].oui, 3) == 0,
                                "duplicate OUI in base table");
    }
  }
}

TEST_CASE("substring search is case-insensitive", "[surv][match]") {
  TEST_ASSERT_TRUE(surv_match_contains_ci("FLOCK_CAM_0032", "flock"));
  TEST_ASSERT_TRUE(surv_match_contains_ci("mi-FlOcK-red", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("floc", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci(NULL, "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("flock", NULL));
}

TEST_CASE("Flock SSID matches with 5 points and tier 0", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("Flock_CAM_0032");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("camera SSID matches with 2 points", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("casa-ipcam-01");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
  TEST_ASSERT_EQUAL_UINT8(2, e->points);
}

TEST_CASE("ordinary SSID does not match", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("INFINITUM1234"));
}

// Regression: these two violated the zero-false-positive ALPR criterion.
TEST_CASE("consumer Motorola modem is NOT ALPR", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("MOTOROLA-A1B2"));
}

TEST_CASE("LPR printer is NOT ALPR", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("HP_LPR_Printer"));
}
