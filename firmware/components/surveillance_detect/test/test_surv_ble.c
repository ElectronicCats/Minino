// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include "surv_test.h"

TEST_CASE("detecta un AirTag por mfr data 004C subtipo 12", "[surv][ble]") {
  // len=0x06 (AD type + 5 bytes de payload), AD type=0xFF,
  // company=0x004C (LE), subtipo=0x12
  const uint8_t adv[] = {0x06, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AIRTAG, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(4, hits[0].points);
}

TEST_CASE("detecta un iBeacon por 4C 00 02 15", "[surv][ble]") {
  // len=0x09 (AD type + 8 bytes de payload)
  const uint8_t adv[] = {0x09, 0xff, 0x4c, 0x00, 0x02,
                         0x15, 0x00, 0x00, 0x00, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_IBEACON, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(2, hits[0].points);
}

TEST_CASE("detecta Tile por service UUID FEED", "[surv][ble]") {
  // len=0x03, AD type=0x16 (service data), UUID 0xFEED little-endian
  const uint8_t adv[] = {0x03, 0x16, 0xed, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("detecta Ray-Ban Meta por service UUID FD5F", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("detecta ODID BLE por service UUID FFFA", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0xfa, 0xff};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_ODID, hits[0].klass);
}

TEST_CASE("detecta un skimmer HC-05 por nombre exacto", "[surv][ble]") {
  // len=0x06, AD type=0x09 (nombre completo), "HC-05"
  const uint8_t adv[] = {0x06, 0x09, 'H', 'C', '-', '0', '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_SKIMMER, hits[0].klass);
}

TEST_CASE("detecta Flock BLE por substring en el nombre", "[surv][ble]") {
  const uint8_t adv[] = {0x0f, 0x09, 'F', 'S', ' ', 'E', 'x', 't',
                         ' ',  'B',  'a', 't', 't', 'e', 'r', 'y'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("un advertisement truncado no desborda", "[surv][ble]") {
  // longitud declarada mayor que el buffer real
  const uint8_t adv[] = {0x1f, 0xff, 0x4c};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("un advertisement sin firma conocida devuelve 0", "[surv][ble]") {
  const uint8_t adv[] = {0x02, 0x01, 0x06};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("detecta Axon por el OUI de la MAC BLE", "[surv][ble]") {
  surv_match_init();
  const uint8_t bda[6] = {0x00, 0x25, 0xdf, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(bda);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AXON, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("dos clases distintas producen dos hits", "[surv][ble]") {
  // Tile (0xFEED) y Ray-Ban Meta (0xFD5F) en dos estructuras AD.
  const uint8_t adv[] = {0x03, 0x03, 0xed, 0xfe, 0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[1].klass);
}

TEST_CASE("dos UUID de la misma clase producen un solo hit", "[surv][ble]") {
  // 0xFEED y 0xFEEC son ambos Tile: push_hit debe deduplicar por clase.
  const uint8_t adv[] = {0x03, 0x03, 0xed, 0xfe, 0x03, 0x03, 0xec, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

// AD type 0x03 es una LISTA de UUIDs de 16 bits: puede traer varios en la
// misma estructura, a diferencia de 0x16 (service data), donde solo los
// primeros dos bytes son UUID y el resto es payload.
TEST_CASE("una lista de UUIDs con dos entradas produce dos hits",
          "[surv][ble]") {
  // Una sola estructura AD, tipo 0x03, con Tile (0xFEED) y Ray-Ban Meta
  // (0xFD5F) empaquetados uno detras del otro.
  const uint8_t adv[] = {0x05, 0x03, 0xed, 0xfe, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(2, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[1].klass);
}

// 0x16 es service DATA, no una lista: solo los primeros dos bytes son UUID,
// el resto es el payload del servicio y no debe leerse como otro UUID.
TEST_CASE("service data 0x16 no lee el payload como una lista de UUIDs",
          "[surv][ble]") {
  // UUID Tile (0xFEED) seguido de bytes de payload que, leidos como UUID,
  // matchean Ray-Ban Meta (0xFD5F). Si 0x16 se tratara como lista, esto daria
  // dos hits en vez de uno.
  const uint8_t adv[] = {0x05, 0x16, 0xed, 0xfe, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("cinco clases se recortan al tope de SURV_BLE_MAX_HITS",
          "[surv][ble]") {
  // GLASSES, ODID, SMARTTAG, TILE y SKIMMER: cinco clases distintas.
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd, 0x03, 0x03, 0xfa, 0xff,
                         0x03, 0x03, 0x5a, 0xfd, 0x03, 0x03, 0xed, 0xfe,
                         0x06, 0x09, 'H',  'C',  '-',  '0',  '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(SURV_BLE_MAX_HITS, n);
}
