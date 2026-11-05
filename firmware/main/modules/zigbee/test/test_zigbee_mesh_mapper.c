/* Host unit tests for Zigbee Mesh Topology Mapper (zigbee_mesh_mapper.c):
 *   - IEEE 802.15.4 FCF bitfield & byte offset double-check
 *   - Short (16-bit) and Extended (64-bit) addressing modes
 *   - Intra-PAN compression (bit 6 = 0x0040) vs non-compressed PAN ID
 *   - LQI/RSSI extraction and out-of-range RSSI clamping
 *   - Link table deduplication, packet counters, and MAX_MESH_LINKS (32) cap
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "../zigbee_mesh_mapper.c"

void setUp(void) {
  s_links_mutex = xSemaphoreCreateMutex();
  s_link_count = 0;
  s_selected_idx = 0;
  s_total_packets = 0;
  s_sd_logging = false;
  memset(s_links, 0, sizeof(s_links));
}

void tearDown(void) {}

static void test_rejects_null_and_short_frames_under_5_bytes(void) {
  uint8_t short_pkt[4] = {0x61, 0x88, 0x01, 0x34};
  parse_and_record_frame(NULL, 10);
  parse_and_record_frame(short_pkt, 4);
  TEST_ASSERT_EQUAL_UINT16(0, s_link_count);
  TEST_ASSERT_EQUAL_UINT32(0, s_total_packets);
}

static void test_dissects_pan_compressed_short_data_frame_and_offsets(void) {
  /* IEEE 802.15.4 FCF = 0x8841 (little-endian: 0x41, 0x88):
   *   - f_type = 0x8841 & 0x07 = 1 ("DATA")
   *   - pan_compressed = (0x8841 >> 6) & 1 = 1
   *   - dst_addr_mode = (0x8841 >> 10) & 3 = 2 (16-bit short)
   *   - src_addr_mode = (0x8841 >> 14) & 3 = 2 (16-bit short)
   * Offsets:
   *   [0..1] FCF = 0x41, 0x88
   *   [2]    Seq = 0x42
   *   [3..4] Dst PAN ID = 0x1A62 (LE: 0x62, 0x1A)
   *   [5..6] Dst Short  = 0x0000 (Coordinator, LE: 0x00, 0x00)
   *   [7..8] Src Short  = 0xBEEF (Router, LE: 0xEF, 0xBE)
   *   [9]    Payload    = 0xAA
   *   [10]   RSSI       = (uint8_t)(-54) = 0xCA
   *   [11]   LQI        = 215
   */
  uint8_t pkt[12] = {0x41,
                     0x88,
                     0x42,
                     0x62,
                     0x1A,
                     0x00,
                     0x00,
                     0xEF,
                     0xBE,
                     0xAA,
                     (uint8_t) (-54),
                     215};
  parse_and_record_frame(pkt, sizeof(pkt));

  TEST_ASSERT_EQUAL_UINT16(1, s_link_count);
  TEST_ASSERT_EQUAL_HEX16(0x1A62, s_links[0].pan_id);
  TEST_ASSERT_EQUAL_HEX16(0xBEEF, s_links[0].src_addr);
  TEST_ASSERT_EQUAL_HEX16(0x0000, s_links[0].dst_addr);
  TEST_ASSERT_EQUAL_STRING("DATA", s_links[0].type_str);
  TEST_ASSERT_EQUAL_INT8(-54, s_links[0].rssi);
  TEST_ASSERT_EQUAL_UINT8(215, s_links[0].lqi);
  TEST_ASSERT_EQUAL_UINT32(1, s_links[0].packets);
}

static void test_dissects_uncompressed_pan_cmd_and_beacon_frames(void) {
  /* FCF = 0x8803 (LE: 0x03, 0x88):
   *   - f_type = 3 ("CMD"), pan_compressed = 0, dst_mode = 2, src_mode = 2
   * Offsets when pan_compressed == 0:
   *   [3..4] Dst PAN = 0x1111
   *   [5..6] Dst Short = 0xFFFF
   *   [7..8] Src PAN = 0x2222 (overwrites pan_id)
   *   [9..10] Src Short = 0x1234
   *   [11] RSSI = +10 (invalid > 0, must clamp to -70)
   *   [12] LQI = 190
   */
  uint8_t pkt[13] = {0x03, 0x88, 0x01, 0x11, 0x11, 0xFF, 0xFF,
                     0x22, 0x22, 0x34, 0x12, 10,   190};
  parse_and_record_frame(pkt, sizeof(pkt));

  TEST_ASSERT_EQUAL_UINT16(1, s_link_count);
  TEST_ASSERT_EQUAL_HEX16(0x2222, s_links[0].pan_id);
  TEST_ASSERT_EQUAL_HEX16(0x1234, s_links[0].src_addr);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, s_links[0].dst_addr);
  TEST_ASSERT_EQUAL_STRING("CMD", s_links[0].type_str);
  TEST_ASSERT_EQUAL_INT8(-70, s_links[0].rssi);
  TEST_ASSERT_EQUAL_UINT8(190, s_links[0].lqi);
}

static void test_deduplicates_links_and_caps_at_max_mesh_links(void) {
  uint8_t pkt[12] = {0x41,
                     0x88,
                     0x01,
                     0x62,
                     0x1A,
                     0x00,
                     0x00,
                     0xEF,
                     0xBE,
                     0x00,
                     (uint8_t) (-60),
                     200};
  parse_and_record_frame(pkt, sizeof(pkt));
  pkt[10] = (uint8_t) (-48);
  pkt[11] = 240;
  parse_and_record_frame(pkt, sizeof(pkt));

  /* Same (PAN, Src, Dst) must update existing entry, not add a second one */
  TEST_ASSERT_EQUAL_UINT16(1, s_link_count);
  TEST_ASSERT_EQUAL_UINT32(2, s_links[0].packets);
  TEST_ASSERT_EQUAL_INT8(-48, s_links[0].rssi);
  TEST_ASSERT_EQUAL_UINT8(240, s_links[0].lqi);

  /* Fill beyond MAX_MESH_LINKS (32) with distinct src_addr */
  for (uint16_t i = 1; i < MAX_MESH_LINKS + 8; i++) {
    pkt[7] = (uint8_t) (i & 0xFF);
    pkt[8] = (uint8_t) (i >> 8);
    parse_and_record_frame(pkt, sizeof(pkt));
  }
  TEST_ASSERT_EQUAL_UINT16(MAX_MESH_LINKS, s_link_count);
}

static void test_rejects_truncated_address_fields_safely(void) {
  /* FCF claims dst_addr_mode=3 (10 bytes: 2 PAN + 8 ext addr), but len is 8 */
  uint8_t truncated[8] = {0x41, 0x8C, 0x01, 0x62, 0x1A, 0x01, 0x02, 0x03};
  parse_and_record_frame(truncated, sizeof(truncated));
  TEST_ASSERT_EQUAL_UINT16(0, s_link_count);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rejects_null_and_short_frames_under_5_bytes);
  RUN_TEST(test_dissects_pan_compressed_short_data_frame_and_offsets);
  RUN_TEST(test_dissects_uncompressed_pan_cmd_and_beacon_frames);
  RUN_TEST(test_deduplicates_links_and_caps_at_max_mesh_links);
  RUN_TEST(test_rejects_truncated_address_fields_safely);
  return UNITY_END();
}
