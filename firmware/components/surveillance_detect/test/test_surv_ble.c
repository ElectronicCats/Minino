// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include "surv_test.h"

TEST_CASE("detects an AirTag by mfr data 004C subtype 12", "[surv][ble]") {
  // len=0x06 (AD type + 5 payload bytes), AD type=0xFF,
  // company=0x004C (LE), subtype=0x12
  const uint8_t adv[] = {0x06, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AIRTAG, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(4, hits[0].points);
}

TEST_CASE("detects an iBeacon by 4C 00 02 15", "[surv][ble]") {
  // len=0x09 (AD type + 8 payload bytes)
  const uint8_t adv[] = {0x09, 0xff, 0x4c, 0x00, 0x02,
                         0x15, 0x00, 0x00, 0x00, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_IBEACON, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(2, hits[0].points);
}

TEST_CASE("detects Tile by service UUID FEED", "[surv][ble]") {
  // len=0x03, AD type=0x16 (service data), UUID 0xFEED little-endian
  const uint8_t adv[] = {0x03, 0x16, 0xed, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("detects Ray-Ban Meta by service UUID FD5F", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("detects BLE ODID by service UUID FFFA", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0xfa, 0xff};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_ODID, hits[0].klass);
}

TEST_CASE("detects an HC-05 skimmer by exact name", "[surv][ble]") {
  // len=0x06, AD type=0x09 (complete local name), "HC-05"
  const uint8_t adv[] = {0x06, 0x09, 'H', 'C', '-', '0', '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_SKIMMER, hits[0].klass);
}

TEST_CASE("detects Flock BLE by name substring", "[surv][ble]") {
  const uint8_t adv[] = {0x0f, 0x09, 'F', 'S', ' ', 'E', 'x', 't',
                         ' ',  'B',  'a', 't', 't', 'e', 'r', 'y'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("truncated advertisement does not overflow", "[surv][ble]") {
  // declared length larger than actual buffer
  const uint8_t adv[] = {0x1f, 0xff, 0x4c};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("advertisement without known signature returns 0", "[surv][ble]") {
  const uint8_t adv[] = {0x02, 0x01, 0x06};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("detects Axon by BLE MAC OUI", "[surv][ble]") {
  surv_match_init();
  const uint8_t bda[6] = {0x00, 0x25, 0xdf, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(bda);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AXON, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("two distinct classes produce two hits", "[surv][ble]") {
  // Tile (0xFEED) and Ray-Ban Meta (0xFD5F) in two AD structures.
  const uint8_t adv[] = {0x03, 0x03, 0xed, 0xfe, 0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[1].klass);
}

TEST_CASE("two UUIDs of the same class produce a single hit", "[surv][ble]") {
  // 0xFEED and 0xFEEC are both Tile: push_hit must deduplicate by class.
  const uint8_t adv[] = {0x03, 0x03, 0xed, 0xfe, 0x03, 0x03, 0xec, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

// AD type 0x03 is a LIST of 16-bit UUIDs: it may pack several into the same
// structure, unlike 0x16 (service data), where only the first two bytes are a
// UUID and the rest is payload.
TEST_CASE("a UUID list with two entries produces two hits",
          "[surv][ble]") {
  // Single AD structure, type 0x03, with Tile (0xFEED) and Ray-Ban Meta
  // (0xFD5F) packed back-to-back.
  const uint8_t adv[] = {0x05, 0x03, 0xed, 0xfe, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[1].klass);
}

// 0x16 is service DATA, not a list: only the first two bytes are the UUID;
// the rest is the service payload and must not be read as another UUID.
TEST_CASE("service data 0x16 does not read payload as a UUID list",
          "[surv][ble]") {
  // Tile UUID (0xFEED) followed by payload bytes that, if read as a UUID,
  // match Ray-Ban Meta (0xFD5F). Treating 0x16 as a list would yield two hits
  // instead of one.
  const uint8_t adv[] = {0x05, 0x16, 0xed, 0xfe, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("five classes are capped at SURV_BLE_MAX_HITS",
          "[surv][ble]") {
  // GLASSES, ODID, SMARTTAG, TILE, and SKIMMER: five distinct classes.
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd, 0x03, 0x03, 0xfa, 0xff,
                         0x03, 0x03, 0x5a, 0xfd, 0x03, 0x03, 0xed, 0xfe,
                         0x06, 0x09, 'H',  'C',  '-',  '0',  '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(SURV_BLE_MAX_HITS, n);
}
