// SPDX-License-Identifier: GPL-3.0-or-later
// Frame vectors. The base vector is synthetic and reproduces the signature
// documented by DeFlockJoplin. It should be supplemented with real captures
// extracted from evidence pcaps (Task 15) once available.
#include <string.h>

#include "surv_ie.h"
#include "surv_test.h"

// Wildcard SSID + the seven IEs of the primary Flock signature
static const uint8_t FLOCK_IES[] = {
    0x00, 0x00,              // tag 0, len 0 (wildcard)
    0x02, 0x02, 0xaa, 0xbb,  // tag 2
    0x0c, 0x01, 0x00,        // tag 12
    0x7f, 0x08, 0,    0,    0,    0,    0,    0,    0,    0x40,  // tag 127
    0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,  // vendor LiteON
    0x2d, 0x02, 0x00, 0x00,                                // tag 45
    0xbf, 0x02, 0x00, 0x00,                                // tag 191
    0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00,  // vendor WFA
};

TEST_CASE("detects wildcard SSID IE", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(
      1, surv_ie_is_wildcard_probe(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("a directed probe is not a wildcard", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x04, 'c', 'a', 's', 'a'};
  TEST_ASSERT_EQUAL_INT(0, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("without SSID IE returns -1", "[surv][ie]") {
  const uint8_t ies[] = {0x2d, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("primary Flock signature matches", "[surv][ie]") {
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("probe with different IEs does not match", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

// Verifies that trailing bytes after the signature do not break it.
TEST_CASE("signature matches with 4 trailing FCS bytes", "[surv][ie]") {
  uint8_t ies[sizeof(FLOCK_IES) + 4];
  memcpy(ies, FLOCK_IES, sizeof(FLOCK_IES));
  memset(ies + sizeof(FLOCK_IES), 0xAB, 4);  // garbage FCS
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("TLV with impossible length does not hang or overflow",
          "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0xff, 0x01, 0x02};
  (void) surv_ie_matches_flock(ies, sizeof(ies));  // must not crash
  TEST_PASS();
}

TEST_CASE("len 0 and null pointer are handled", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(NULL, 0));
  TEST_ASSERT_FALSE(surv_ie_matches_flock(NULL, 0));
}

TEST_CASE("added signature matches without losing the compiled-in one",
          "[surv][ie]") {
  surv_ie_reset_signatures();
  const uint8_t otra_trama[] = {0x00, 0x00, 0x2d, 0x02, 0x00,
                                0x00, 0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));

  const surv_ie_tok_t otra[] = {{45, 0, {0}}, {191, 0, {0}}};
  TEST_ASSERT_TRUE(surv_ie_add_signature(otra, 2));
  TEST_ASSERT_TRUE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));
  // the compiled-in signature remains active
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));

  surv_ie_reset_signatures();
}

TEST_CASE("overlay cap of 8 signatures is respected", "[surv][ie]") {
  surv_ie_reset_signatures();
  const surv_ie_tok_t t1[] = {{45, 0, {0}}};
  for (int i = 0; i < SURV_IE_MAX_SIGS; i++) {
    TEST_ASSERT_TRUE(surv_ie_add_signature(t1, 1));
  }
  TEST_ASSERT_FALSE(surv_ie_add_signature(t1, 1));
  surv_ie_reset_signatures();
}

TEST_CASE("signature with more tokens than allowed is rejected", "[surv][ie]") {
  surv_ie_reset_signatures();
  surv_ie_tok_t demasiados[SURV_IE_MAX_TOKS + 1];
  memset(demasiados, 0, sizeof(demasiados));
  TEST_ASSERT_FALSE(surv_ie_add_signature(demasiados, SURV_IE_MAX_TOKS + 1));
  TEST_ASSERT_FALSE(surv_ie_add_signature(NULL, 3));
  TEST_ASSERT_FALSE(surv_ie_add_signature(demasiados, 0));
}

// PENDING REAL CAPTURE: the tests above use a synthetic vector. The
// "phantom overflow" and TLV resync cases that flock-you encountered in the
// field cannot be reproduced without real frames. Once Task 15 produces
// evidence pcaps, extract the IEs from a real tier-4 frame and add it here as
// FLOCK_IES_REAL, with its own TEST_CASE.
