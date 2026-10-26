/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host tests for the ESP-SDR burst protocol.
 *
 * The wire format is fixed by the ESP-WebSDR browser viewer, so these tests
 * pin the exact bytes the viewer expects: the replies it parses, the sample
 * formats it decodes and the CRC it checks the payload with.
 */

#include <stdio.h>
#include <string.h>

#include "esdr_burst_proto.h"
#include "unity.h"

void setUp(void) {
  /* Nothing to set up: the protocol has no state. */
}

void tearDown(void) {
  /* Nothing to tear down. */
}

/* --------------------------------------------------------------------------
 * The parser
 * -------------------------------------------------------------------------- */

static void test_info_is_parsed(void) {
  esdr_request_t req;
  TEST_ASSERT_TRUE(esdr_parse_line("INFO", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_INFO, req.cmd);
}

static void test_caps_and_the_other_exact_commands(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("CAPS", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_CAPS, req.cmd);

  TEST_ASSERT_TRUE(esdr_parse_line("VERSION?", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_VERSION, req.cmd);

  TEST_ASSERT_TRUE(esdr_parse_line("TRANSPORT?", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_TRANSPORT, req.cmd);

  TEST_ASSERT_TRUE(esdr_parse_line("RELEASE", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_RELEASE, req.cmd);

  TEST_ASSERT_TRUE(esdr_parse_line("RANGE?", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_RANGE, req.cmd);

  TEST_ASSERT_TRUE(esdr_parse_line("LIMITS?", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_LIMITS, req.cmd);
}

static void test_sync_carries_the_nonce_back(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("SYNC 1700000000000", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_SYNC, req.cmd);
  TEST_ASSERT_EQUAL_UINT64(1700000000000ULL, req.nonce);
}

static void test_freq_carries_the_frequency(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("FREQ 2412", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_FREQ, req.cmd);
  TEST_ASSERT_EQUAL_UINT32(2412, req.freq_mhz);
}

static void test_capture_commands_carry_the_count_and_the_rate(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("CAP16 4096 0", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_CAP16, req.cmd);
  TEST_ASSERT_EQUAL_UINT32(4096, req.samples);
  TEST_ASSERT_EQUAL_UINT32(0, req.rate);
  TEST_ASSERT_EQUAL_INT(ESDR_FORMAT_IQ8, req.format);

  TEST_ASSERT_TRUE(esdr_parse_line("CAP20 16380 6", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_CAP20, req.cmd);
  TEST_ASSERT_EQUAL_UINT32(16380, req.samples);
  TEST_ASSERT_EQUAL_UINT32(6, req.rate);
  TEST_ASSERT_EQUAL_INT(ESDR_FORMAT_IQ10, req.format);
}

static void test_rxrun_carries_every_field(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("RXRUN 4096 1 5 20", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_RXRUN, req.cmd);
  TEST_ASSERT_EQUAL_UINT32(4096, req.samples);
  TEST_ASSERT_EQUAL_UINT32(1, req.rate);
  TEST_ASSERT_EQUAL_UINT32(5, req.repeats);
  TEST_ASSERT_EQUAL_UINT32(20, req.format);
}

static void test_trailing_garbage_is_refused(void) {
  esdr_request_t req;

  /* The upstream firmware uses sscanf with a trailing " %c" to reject a
     line that carries anything else after the numbers, and so does this. */
  TEST_ASSERT_FALSE(esdr_parse_line("FREQ 2412 x", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("CAP20 4096 0 x", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("SYNC 123 x", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("RXRUN 4096 1 5 20 x", &req));
}

static void test_unknown_and_empty_lines_are_refused(void) {
  esdr_request_t req;

  TEST_ASSERT_FALSE(esdr_parse_line("", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("NONSENSE", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("FREQ", &req));
  TEST_ASSERT_FALSE(esdr_parse_line("CAP20 4096", &req));
}

static void test_the_minino_escape_is_parsed(void) {
  esdr_request_t req;

  TEST_ASSERT_TRUE(esdr_parse_line("MININO", &req));
  TEST_ASSERT_EQUAL_INT(ESDR_CMD_MININO, req.cmd);
}

/* --------------------------------------------------------------------------
 * The replies
 * -------------------------------------------------------------------------- */

static void test_the_identity_is_the_one_the_viewer_expects(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_info(reply, sizeof(reply)) > 0);
  /* The viewer applies /^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst
     (\d+)$/ and wants the burst count between 4096 and 16384. */
  TEST_ASSERT_EQUAL_STRING("C6SDR 6 burst 16380\n", reply);
}

static void test_the_caps_only_advertise_what_is_implemented(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_caps(reply, sizeof(reply)) > 0);
  TEST_ASSERT_EQUAL_STRING(
      "CAPS VERSION RXLIMITS HWAGC GAIN IQ8 SERIALLEASE TUNEEXT RX40 RX16 "
      "UARTBAUD "
      "SPEC SPECN SPECCAPS "
      "DCT\n",
      reply);

  /* The capabilities that are not implemented must not be advertised: the
     viewer shows a control for each of them. */
  TEST_ASSERT_NULL(strstr(reply, "GPIO"));
  TEST_ASSERT_NULL(strstr(reply, "ALPF"));
  TEST_ASSERT_NULL(strstr(reply, "LPF"));
  /* The statistics records are not implemented either. */
  TEST_ASSERT_NULL(strstr(reply, "SPECSTAT"));

  /* The ones that are: the viewer needs UARTBAUD to know it is on a USB
     link, and without SPEC it will not offer the on-chip spectra at all. */
  TEST_ASSERT_NOT_NULL(strstr(reply, "UARTBAUD"));
  TEST_ASSERT_NOT_NULL(strstr(reply, "SPEC"));
  TEST_ASSERT_NOT_NULL(strstr(reply, "SPECN"));
  TEST_ASSERT_NOT_NULL(strstr(reply, "SPECCAPS"));
  TEST_ASSERT_NOT_NULL(strstr(reply, "DCT"));
}

static void test_the_transport_is_usb_without_a_baud_rate(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_transport(reply, sizeof(reply)) > 0);
  /* The viewer insists on /^TRANSPORT (UART|USB) (\d+)$/ and on the number
     being zero for a USB link. */
  TEST_ASSERT_EQUAL_STRING("TRANSPORT USB 0\n", reply);
}

static void test_the_sync_reply_echoes_the_nonce(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_sync(reply, sizeof(reply), 1700000000000ULL) > 0);
  TEST_ASSERT_EQUAL_STRING("SYNC 1700000000000\n", reply);
}

static void test_the_tuning_range_is_the_one_the_board_can_reach(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_range(reply, sizeof(reply)) > 0);
  /* The viewer wants "RANGE <low> <high> 1": whole megahertz, and the
     step has to be one. */
  TEST_ASSERT_EQUAL_STRING("RANGE 2412 2472 1\n", reply);
}

static void test_the_limits_are_the_ones_the_viewer_validates(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_limits(reply, sizeof(reply)) > 0);
  /* The viewer parses the JSON after the "LIMITS " prefix and checks the
     shape of every field: gain is [min, max, step] with the step dividing
     the range, bandwidth is null or [min, max, step, default], rates is a
     list of rates it knows and bits is a list of 8 and 10. */
  TEST_ASSERT_EQUAL_STRING(
      "LIMITS {\"gain\":[0,79,1],\"bandwidth\":null,"
      "\"rates\":[80000000,40000000,16000000],\"bits\":[8,10]}\n",
      reply);
  TEST_ASSERT_TRUE(strlen(reply) <= 256);

  TEST_ASSERT_TRUE(esdr_reply_gain(reply, sizeof(reply), false, 40) > 0);
  TEST_ASSERT_EQUAL_STRING("GAIN HARDWARE -1 0 79 0\n", reply);

  TEST_ASSERT_TRUE(esdr_reply_gain(reply, sizeof(reply), true, 40) > 0);
  TEST_ASSERT_EQUAL_STRING("GAIN MANUAL 40 0 79 0\n", reply);
}

static void test_the_version_reply_is_the_json_the_viewer_parses(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(esdr_reply_version(reply, sizeof(reply)) > 0);
  TEST_ASSERT_NOT_NULL(strstr(reply, "VERSION {"));
  TEST_ASSERT_NOT_NULL(strstr(reply, "\"profile\":\"esp32c6\""));
  TEST_ASSERT_NOT_NULL(strstr(reply, "\"revision\":\"unknown\""));

  /* The viewer wants the build date to be the date part of the timestamp,
     and the timestamp to be exactly twenty characters:
     "YYYY-MM-DDTHH:MM:SSZ". */
  const char* date = strstr(reply, "\"build_date\":\"");
  TEST_ASSERT_NOT_NULL(date);
  date += strlen("\"build_date\":\"");
  /* The shape is "YYYY-MM-DD": the year and the month are digits, the
     dashes sit at 4 and 7, and the value ends at the closing quote. */
  for (int j = 0; j < 10; j++) {
    if (j == 4 || j == 7) {
      TEST_ASSERT_EQUAL_INT('-', date[j]);
    } else {
      TEST_ASSERT_TRUE(date[j] >= '0' && date[j] <= '9');
    }
  }
  TEST_ASSERT_EQUAL_INT('"', date[10]);

  const char* stamp = strstr(reply, "\"build_timestamp\":\"");
  TEST_ASSERT_NOT_NULL(stamp);
  stamp += strlen("\"build_timestamp\":\"");
  /* "YYYY-MM-DDTHH:MM:SSZ" is twenty characters, so the Z is the last
     one and the closing quote follows it. */
  TEST_ASSERT_EQUAL_INT('Z', stamp[19]);
  TEST_ASSERT_EQUAL_INT('"', stamp[20]);
  TEST_ASSERT_EQUAL_STRING_LEN(date, stamp, 10);
}

static void test_the_data_header_is_the_one_the_viewer_parses(void) {
  char reply[ESDR_REPLY_MAX];

  TEST_ASSERT_TRUE(
      esdr_reply_data(reply, sizeof(reply), 4096, 0xdeadbeefu, 217) > 0);
  /* The viewer applies /^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/ and compares
     the CRC it computes over the payload with the one in the header. */
  TEST_ASSERT_EQUAL_STRING("DATA 4096 deadbeef 217\n", reply);
}

/* --------------------------------------------------------------------------
 * The sample formats
 * -------------------------------------------------------------------------- */

/* The dump word carries I in bits 9-0 and Q in bits 19-10, both signed ten
   bit values. A word with I = -512 and Q = 511 is therefore
   0x1FF << 10 | 0x200 = 0x7FE00. */
static void test_the_eight_bit_format_is_i_then_q(void) {
  uint32_t words[1] = {0x7FE00u};

  TEST_ASSERT_EQUAL_UINT32(2, esdr_pack_iq8(words, 1));
  /* I = -512 >> 2 = -128 = 0x80, Q = 511 >> 2 = 127 = 0x7F. */
  TEST_ASSERT_EQUAL_HEX8(0x80, ((uint8_t*) words)[0]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, ((uint8_t*) words)[1]);
}

static void test_the_ten_bit_format_is_twenty_bits_per_pair(void) {
  uint32_t words[2] = {0x7FE00u, 0x00000u};
  uint8_t* out = (uint8_t*) words;

  TEST_ASSERT_EQUAL_UINT32(5, esdr_pack_iq10(words, 2));
  /* The twenty bits of the word go out least significant bit first, so the
     viewer reads I in bits 0-9 and Q in bits 10-19 of the group. */
  TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFE, out[1]);
  TEST_ASSERT_EQUAL_HEX8(0x07, out[2]);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[3]);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[4]);
}

static void test_the_ten_bit_format_decodes_the_way_the_viewer_does(void) {
  uint32_t words[2] = {0x7FE00u, 0x00000u};
  uint8_t* bytes = (uint8_t*) words;

  esdr_pack_iq10(words, 2);

  /* The viewer accumulates the bytes into twenty bit groups and reads I
     from the low ten bits and Q from the high ten. */
  uint32_t group = 0;
  for (int j = 0; j < 3; j++) {
    group |= (uint32_t) bytes[j] << (8 * j);
  }
  /* The viewer sign extends the ten bit fields: "((word & 1023) << 22) >> 22"
     for I and the same for Q out of bits 10-19. */
  TEST_ASSERT_EQUAL_INT(-512, (int) (((int32_t) (group & 0x3FFu) << 22) >> 22));
  TEST_ASSERT_EQUAL_INT(
      511, (int) ((((int32_t) ((group >> 10) & 0x3FFu)) << 22) >> 22));
}

static void test_an_odd_tail_keeps_the_five_byte_group(void) {
  uint32_t words[3] = {0x7FE00u, 0x00000u, 0x00000u};
  uint8_t* out = (uint8_t*) words;

  /* Three samples: one full group of five bytes and a tail of three. */
  TEST_ASSERT_EQUAL_UINT32(8, esdr_pack_iq10(words, 3));
  TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0xFE, out[1]);
  TEST_ASSERT_EQUAL_HEX8(0x07, out[2]);
}

static void test_the_wire_size_matches_the_payload_the_viewer_reads(void) {
  /* The viewer reads ceil(n * bits * 2 / 8) bytes. */
  TEST_ASSERT_EQUAL_UINT32(8192, esdr_wire_size(4096, ESDR_FORMAT_IQ8));
  TEST_ASSERT_EQUAL_UINT32(10240, esdr_wire_size(4096, ESDR_FORMAT_IQ10));
  TEST_ASSERT_EQUAL_UINT32(40950, esdr_wire_size(16380, ESDR_FORMAT_IQ10));
  /* An odd count leaves three bytes in the last group. */
  TEST_ASSERT_EQUAL_UINT32(8, esdr_wire_size(3, ESDR_FORMAT_IQ10));
}

/* --------------------------------------------------------------------------
 * The CRC
 * -------------------------------------------------------------------------- */

static void test_the_crc_is_the_one_the_viewer_computes(void) {
  /* The browser uses the zlib crc32, so the vectors have to match it. */
  TEST_ASSERT_EQUAL_HEX32(0x00000000u, esdr_crc32((const uint8_t*) "", 0));
  TEST_ASSERT_EQUAL_HEX32(0xE8B7BE43u, esdr_crc32((const uint8_t*) "a", 1));
  TEST_ASSERT_EQUAL_HEX32(0x352441C2u, esdr_crc32((const uint8_t*) "abc", 3));
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u,
                          esdr_crc32((const uint8_t*) "123456789", 9));
  TEST_ASSERT_EQUAL_HEX32(
      0x414FA339u,
      esdr_crc32((const uint8_t*) "The quick brown fox jumps over the lazy dog",
                 43));
}

static void test_the_crc_covers_the_whole_payload(void) {
  uint8_t data[4] = {0x00, 0x01, 0x02, 0x03};

  TEST_ASSERT_NOT_EQUAL(esdr_crc32(data, 3), esdr_crc32(data, 4));
  /* The CRC of a payload does not depend on what came before it. */
  uint8_t other[5] = {0xAA, 0x00, 0x01, 0x02, 0x03};
  TEST_ASSERT_EQUAL_HEX32(esdr_crc32(data, 4), esdr_crc32(other + 1, 4));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_info_is_parsed);
  RUN_TEST(test_caps_and_the_other_exact_commands);
  RUN_TEST(test_sync_carries_the_nonce_back);
  RUN_TEST(test_freq_carries_the_frequency);
  RUN_TEST(test_capture_commands_carry_the_count_and_the_rate);
  RUN_TEST(test_rxrun_carries_every_field);
  RUN_TEST(test_trailing_garbage_is_refused);
  RUN_TEST(test_unknown_and_empty_lines_are_refused);
  RUN_TEST(test_the_minino_escape_is_parsed);

  RUN_TEST(test_the_identity_is_the_one_the_viewer_expects);
  RUN_TEST(test_the_caps_only_advertise_what_is_implemented);
  RUN_TEST(test_the_transport_is_usb_without_a_baud_rate);
  RUN_TEST(test_the_sync_reply_echoes_the_nonce);
  RUN_TEST(test_the_tuning_range_is_the_one_the_board_can_reach);
  RUN_TEST(test_the_limits_are_the_ones_the_viewer_validates);
  RUN_TEST(test_the_version_reply_is_the_json_the_viewer_parses);
  RUN_TEST(test_the_data_header_is_the_one_the_viewer_parses);

  RUN_TEST(test_the_eight_bit_format_is_i_then_q);
  RUN_TEST(test_the_ten_bit_format_is_twenty_bits_per_pair);
  RUN_TEST(test_the_ten_bit_format_decodes_the_way_the_viewer_does);
  RUN_TEST(test_an_odd_tail_keeps_the_five_byte_group);
  RUN_TEST(test_the_wire_size_matches_the_payload_the_viewer_reads);

  RUN_TEST(test_the_crc_is_the_one_the_viewer_computes);
  RUN_TEST(test_the_crc_covers_the_whole_payload);

  return UNITY_END();
}
