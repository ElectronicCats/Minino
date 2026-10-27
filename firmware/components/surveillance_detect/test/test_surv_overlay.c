// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_ie.h"
#include "surv_match.h"
#include "surv_overlay.h"
#include "surv_test.h"

TEST_CASE("added OUI starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  surv_match_init();
  const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("removed OUI stops matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("-oui,70:c9:4e\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.removed);
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("comments and empty lines are ignored", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("# comment\n\n   \n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(0, st.removed);
  TEST_ASSERT_EQUAL_UINT16(0, st.skipped);
}

TEST_CASE("malformed line is counted as skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(
      "+oui,notamac,flock,5,2\n"
      "+oui,aa:bb\n"
      "garbage\n"
      "+oui,dd:ee:ff,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  TEST_ASSERT_EQUAL_UINT16(3, st.skipped);
}

TEST_CASE("null or empty text does not fail", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(NULL);
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  st = surv_overlay_parse("");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
}

TEST_CASE("added SSID keyword starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+ssid,mikamara,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const surv_kw_entry_t* e = surv_match_ssid("red-MiKaMaRa-2");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
}

TEST_CASE("added UUID starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+uuid,fd6f,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t adv[] = {0x03, 0x03, 0x6f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(1, surv_match_ble_adv(adv, sizeof(adv), hits));
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, hits[0].klass);
}

TEST_CASE("overlay IE signature is added to the compiled-in one",
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

TEST_CASE("malformed IE signature is skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221:abc\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// vlen is vestigial: the matcher decides to compare vendor bytes by checking
// tag==221 (SURV_IE_VENDOR_TAG) alone, never vlen. A bare "221" token without
// the 14-hex payload would "parse" fine and then silently never match anything
// if not rejected here.
TEST_CASE("token 221 without vendor payload is skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// atoi("xyz") silently returns 0, and a token with tag 0 is indistinguishable
// from the SSID element: tokens_match_sig skips it BEFORE comparing against
// the signature, so the signature never completes and remains dead while also
// occupying one of the 8 overlay slots. Same failure mode as bare 221 without
// payload.
TEST_CASE("tag 0 token is skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,0,45\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("non-numeric token is skipped instead of being read as tag 0",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,xyz,45\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// A signature with more tokens than fit in SURV_IE_MAX_TOKS is skipped in its
// entirety: applying a truncated signature would load something the user did
// not write, which would also be a silent error.
TEST_CASE("iesig signature with more tokens than allowed is skipped entirely",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st =
      surv_overlay_parse("+iesig,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// parse_oui must require exactly "xx:xx:xx": accepting a full MAC where an OUI
// was expected makes the user believe they targeted a single device when they
// actually targeted an entire vendor prefix.
TEST_CASE("OUI with a full MAC is rejected instead of truncated",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st =
      surv_overlay_parse("+oui,aa:bb:cc:dd:ee:ff,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
  surv_match_init();
  const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("OUI with trailing garbage on third octet is rejected",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:ccdd,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// atoi("xyz") silently returns 0: a non-numeric points field would load a
// 0-point entry, invisible and inert, instead of counting as skipped (which
// the user can see on screen). parse_ie_token already solves this with strtol
// plus end-pointer checking; these four tests cover the other places where the
// same bug existed: +oui, +ssid, and +uuid.
TEST_CASE("+oui with non-numeric points is skipped instead of loading 0",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,xyz,2\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("+oui with non-numeric tier is skipped instead of loading 0",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,5,xyz\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("+ssid with non-numeric points is skipped instead of loading 0",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+ssid,mikamara,cam,xyz\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("+uuid with non-numeric points is skipped instead of loading 0",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+uuid,fd6f,cam,xyz\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

// Total length (8) alone is not enough: if the last octet has garbage attached
// to a single valid hex digit, "aa:bb:c!" is also 8 characters long. Only
// sscanf's %n (how many characters the last matched field actually consumed)
// detects that the third octet was incomplete. Without that check, this form
// would slip through with the same bug as the full MAC or "aa:bb:ccdd", just
// narrower.
TEST_CASE("8-character OUI with garbage in the last octet is rejected",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:c!,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("cap of 256 OUIs is respected", "[surv][overlay]") {
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
