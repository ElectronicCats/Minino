#include "unity.h"
#include "warthread_parser.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_null_and_short_frames_rejected(void) {
  warthread_parsed_frame_t parsed;
  uint8_t buffer[32] = {0};

  TEST_ASSERT_FALSE(warthread_parse_frame(NULL, sizeof(buffer), &parsed));
  TEST_ASSERT_FALSE(warthread_parse_frame(buffer, sizeof(buffer), NULL));
  TEST_ASSERT_FALSE(warthread_parse_frame(buffer, 15, &parsed));
}

void test_ieee802154_data_frame_parsed(void) {
  // 802.15.4 frame: FCF=0xE841 (data, PAN compressed, dst short, src extended),
  // Seq=0x12, DstPAN=0x7668, DstAddr=0xffff,
  // ExtAddr=0x5a,0xd2,0xcf,0x46,0xfc,0xae,0x75,0xd7
  uint8_t psdu[20] = {
      0x41, 0xe8,              // FCF (little-endian 0xE841)
      0x12,                    // Sequence number
      0x68, 0x76,              // Destination PAN (0x7668)
      0xff, 0xff,              // Destination short address (0xffff)
      0xd7, 0x75, 0xae, 0xfc,  // Extended source (little-endian on wire)
      0x46, 0xcf, 0xd2, 0x5a, 0x01, 0x02, 0x03, 0x04  // Payload
  };

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_TRUE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
  TEST_ASSERT_EQUAL_HEX16(0xe841, parsed.frame_control);
  TEST_ASSERT_EQUAL_HEX16(0x7668, parsed.destination_pan);
  TEST_ASSERT_EQUAL_HEX16(0xffff, parsed.destination);
  TEST_ASSERT_FALSE(parsed.is_mle);
  TEST_ASSERT_EQUAL_STRING(PROTOCOL_TYPE_IEEE, parsed.protocol_type);
  TEST_ASSERT_EQUAL_STRING("5a:d2:cf:46:fc:ae:75:d7", parsed.extd_source_str);
}

void test_short_source_frames_are_rejected(void) {
  // FCF=0x9869 has a 16-bit source address, which this parser does not model.
  uint8_t psdu[20] = {
      0x69, 0x98, 0x12, 0x68, 0x76, 0xff, 0xff, 0x01,
      0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  };

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_FALSE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
}

void test_non_compressed_source_pan_is_skipped(void) {
  // FCF=0xD801: data, PAN not compressed, dst short, src extended.
  uint8_t psdu[22] = {
      0x01, 0xd8, 0x33, 0x68, 0x76, 0xff, 0xff,  // FCF, seq, dst PAN/addr
      0xaa, 0xbb,                                // Source PAN
      0xd7, 0x75, 0xae, 0xfc, 0x46, 0xcf, 0xd2, 0x5a, 0x01, 0x02, 0x03, 0x04,
  };

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_TRUE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
  TEST_ASSERT_EQUAL_HEX16(0xd801, parsed.frame_control);
  TEST_ASSERT_EQUAL_STRING("5a:d2:cf:46:fc:ae:75:d7", parsed.extd_source_str);
}

void test_thread_mle_packet_parsed(void) {
  // Thread MLE frame: FCF=0xd841, Seq=0x42, DstPAN=0xb24b, DstAddr=0xffff,
  // ExtAddr=ae:27:c5:e6:1e:d7:3c:e3,
  // 3 bytes dispatch header, UDP src=19788 (0x4d4c), UDP dst=19788 (0x4d4c)
  uint8_t psdu[28] = {
      0x41, 0xd8,              // FCF (little-endian 0xd841)
      0x42,                    // Seq
      0x4b, 0xb2,              // DstPAN (0xb24b)
      0xff, 0xff,              // DstAddr (0xffff)
      0xe3, 0x3c, 0xd7, 0x1e,  // Extended source
      0xe6, 0xc5, 0x27, 0xae,
      0x00, 0x01, 0x02,       // Dispatch / 6LoWPAN mesh headers (3 bytes)
      0x4d, 0x4c,             // UDP Source Port 19788 (big-endian)
      0x4d, 0x4c,             // UDP Destination Port 19788 (big-endian)
      0xaa, 0xbb, 0xcc, 0xdd  // MLE Payload
  };

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_TRUE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
  TEST_ASSERT_EQUAL_HEX16(0xd841, parsed.frame_control);
  TEST_ASSERT_EQUAL_HEX16(0xb24b, parsed.destination_pan);
  TEST_ASSERT_EQUAL_HEX16(0xffff, parsed.destination);
  TEST_ASSERT_TRUE(parsed.is_mle);
  TEST_ASSERT_EQUAL_STRING(PROTOCOL_TYPE_MLE, parsed.protocol_type);
  TEST_ASSERT_EQUAL_STRING("ae:27:c5:e6:1e:d7:3c:e3", parsed.extd_source_str);
  TEST_ASSERT_EQUAL_UINT16(19788, parsed.udp_source_port);
  TEST_ASSERT_EQUAL_UINT16(19788, parsed.udp_destination_port);
}

void test_thread_mle_truncated_payload_rejected(void) {
  // Truncated MLE frame lacking full UDP header
  uint8_t psdu[18] = {0x41, 0xd8, 0x42, 0x4b, 0xb2, 0xff, 0xff, 0xe3, 0x3c,
                      0xd7, 0x1e, 0xe6, 0xc5, 0x27, 0xae, 0x00, 0x01, 0x02};

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_FALSE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
}

void test_thread_mle_custom_udp_ports(void) {
  // Thread MLE with custom UDP ports (e.g., 1234 -> 5678)
  uint8_t psdu[26] = {0x41, 0xd8, 0x01, 0xaa, 0xbb, 0x12, 0x34,
                      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                      0x88, 0x00, 0x00, 0x00, 0x04, 0xd2,  // Port 1234
                      0x16, 0x2e,                          // Port 5678
                      0x01, 0x02, 0x03};

  warthread_parsed_frame_t parsed;
  TEST_ASSERT_TRUE(warthread_parse_frame(psdu, sizeof(psdu), &parsed));
  TEST_ASSERT_EQUAL_UINT16(1234, parsed.udp_source_port);
  TEST_ASSERT_EQUAL_UINT16(5678, parsed.udp_destination_port);
  TEST_ASSERT_EQUAL_STRING("88:77:66:55:44:33:22:11", parsed.extd_source_str);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_null_and_short_frames_rejected);
  RUN_TEST(test_ieee802154_data_frame_parsed);
  RUN_TEST(test_short_source_frames_are_rejected);
  RUN_TEST(test_non_compressed_source_pan_is_skipped);
  RUN_TEST(test_thread_mle_packet_parsed);
  RUN_TEST(test_thread_mle_truncated_payload_rejected);
  RUN_TEST(test_thread_mle_custom_udp_ports);
  return UNITY_END();
}
