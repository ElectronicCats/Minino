/* Host tests for the RF Spectrum channel-range helpers.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Why these exist
 * ---------------
 * current_channel is used as an index into channel_rssi[], channel_peak[],
 * channel_packets[], deauth_count[], jammer_alert[] and drone_count[], and it
 * drives the UP/DOWN tuner. Zigbee tunes 11..26 while those arrays were sized
 * for Wi-Fi's 1..14, and rf_spectrum_set_protocol() set current_channel = 15
 * straight away: from the next 20 ms tick the sampling task and every renderer
 * read past the end of each array.
 *
 * These tests pin the bounds so a future protocol, or a stray preference
 * value, cannot reintroduce the out-of-bounds reads.
 */

#include "rf_spectrum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* --- the storage invariant --------------------------------------------- */

static void test_storage_covers_the_widest_channel_plan(void) {
  /* Anything the tuner can produce must be indexable. */
  /* Unity's macro reads (threshold, actual): assert Zigbee's top >= Wi-Fi's. */
  TEST_ASSERT_GREATER_OR_EQUAL(RF_WIFI_MAX_CHANNELS, RF_ZIGBEE_MAX_CHANNEL);
  TEST_ASSERT_EQUAL(RF_ZIGBEE_MAX_CHANNEL, RF_MAX_CHANNELS);
  TEST_ASSERT_LESS_OR_EQUAL(255, RF_MAX_CHANNELS);
}

static void test_indexable_range_is_exactly_one_to_max(void) {
  TEST_ASSERT_FALSE(rf_channel_is_indexable(0));
  TEST_ASSERT_TRUE(rf_channel_is_indexable(1));
  TEST_ASSERT_TRUE(rf_channel_is_indexable(RF_WIFI_MAX_CHANNELS));
  TEST_ASSERT_TRUE(rf_channel_is_indexable(RF_MAX_CHANNELS));
  TEST_ASSERT_FALSE(rf_channel_is_indexable(RF_MAX_CHANNELS + 1));
  TEST_ASSERT_FALSE(rf_channel_is_indexable(255));
}

/* --- per-protocol bounds ----------------------------------------------- */

static void test_wifi_bounds(void) {
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_min(RF_PROTOCOL_WIFI));
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS, rf_channel_max(RF_PROTOCOL_WIFI));
}

static void test_zigbee_bounds(void) {
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MIN_CHANNEL,
                          rf_channel_min(RF_PROTOCOL_ZIGBEE));
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MAX_CHANNEL,
                          rf_channel_max(RF_PROTOCOL_ZIGBEE));
}

static void test_unknown_protocol_falls_back_to_the_wifi_plan(void) {
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_min((rf_protocol_t)99));
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS,
                          rf_channel_max((rf_protocol_t)99));
}

/* --- clamping ---------------------------------------------------------- */

static void test_clamp_leaves_valid_channels_alone(void) {
  for (uint8_t ch = 1; ch <= RF_WIFI_MAX_CHANNELS; ch++) {
    TEST_ASSERT_EQUAL_UINT8(ch, rf_channel_clamp(RF_PROTOCOL_WIFI, ch));
  }
  for (uint8_t ch = RF_ZIGBEE_MIN_CHANNEL; ch <= RF_ZIGBEE_MAX_CHANNEL; ch++) {
    TEST_ASSERT_EQUAL_UINT8(ch, rf_channel_clamp(RF_PROTOCOL_ZIGBEE, ch));
  }
}

static void test_clamp_pulls_out_of_range_channels_back_in(void) {
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_clamp(RF_PROTOCOL_WIFI, 0));
  /* Clamping saturates at the band's edge. */
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS,
                          rf_channel_clamp(RF_PROTOCOL_WIFI, 26));
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS,
                          rf_channel_clamp(RF_PROTOCOL_WIFI, 255));

  /* 15 is a valid Zigbee channel but not a Wi-Fi one. */
  TEST_ASSERT_EQUAL_UINT8(15, rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 15));
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS,
                          rf_channel_clamp(RF_PROTOCOL_WIFI, 15));
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MIN_CHANNEL,
                          rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 1));
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MIN_CHANNEL,
                          rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 0));
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MAX_CHANNEL,
                          rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 255));
}

static void test_clamp_output_is_always_indexable(void) {
  const rf_protocol_t protocols[] = {RF_PROTOCOL_WIFI, RF_PROTOCOL_ZIGBEE};
  for (unsigned p = 0; p < 2; p++) {
    for (int ch = 0; ch <= 255; ch++) {
      uint8_t clamped = rf_channel_clamp(protocols[p], (uint8_t)ch);
      TEST_ASSERT_TRUE_MESSAGE(rf_channel_is_indexable(clamped),
                               "every clamp result must be safe to index");
    }
  }
}

/* --- the regression ---------------------------------------------------- */

static void test_switching_to_zigbee_does_not_select_an_unindexable_channel(void) {
  /* rf_spectrum_set_protocol(RF_PROTOCOL_ZIGBEE) used to hard-code 15, which
     sat one past the end of the Wi-Fi-sized arrays. */
  uint8_t ch = rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 15);
  TEST_ASSERT_TRUE(rf_channel_is_indexable(ch));
}

static void test_tuner_cannot_walk_out_of_the_band(void) {
  /* Walk the UP/DOWN tuner across every key press and assert the channel is
     indexable and inside the active protocol's band at every step. */
  const rf_protocol_t protocols[] = {RF_PROTOCOL_WIFI, RF_PROTOCOL_ZIGBEE};

  for (unsigned p = 0; p < 2; p++) {
    rf_protocol_t proto = protocols[p];
    uint8_t min_ch = rf_channel_min(proto);
    uint8_t max_ch = rf_channel_max(proto);
    uint8_t ch = (proto == RF_PROTOCOL_ZIGBEE) ? 15 : 1;

    for (int press = 0; press < 200; press++) {
      /* UP, as rf_spectrum_input_cb() does it. */
      ch = (ch >= max_ch) ? min_ch : (uint8_t)(ch + 1);
      TEST_ASSERT_TRUE(rf_channel_is_indexable(ch));
      TEST_ASSERT_GREATER_OR_EQUAL(min_ch, ch);
      TEST_ASSERT_LESS_OR_EQUAL(max_ch, ch);

      /* DOWN. */
      ch = (ch <= min_ch) ? max_ch : (uint8_t)(ch - 1);
      TEST_ASSERT_TRUE(rf_channel_is_indexable(ch));
      TEST_ASSERT_GREATER_OR_EQUAL(min_ch, ch);
      TEST_ASSERT_LESS_OR_EQUAL(max_ch, ch);
    }
  }
}

static void test_switching_protocols_reclaims_the_channel(void) {
  /* A channel selected in one band must not survive into the other as an
     unindexable value. */
  uint8_t ch = 26; /* top of Zigbee */
  TEST_ASSERT_TRUE(rf_channel_is_indexable(ch));

  /* Switching to Wi-Fi must pull it back inside 1..14. */
  ch = rf_channel_clamp(RF_PROTOCOL_WIFI, ch);
  TEST_ASSERT_EQUAL_UINT8(RF_WIFI_MAX_CHANNELS, ch);

  /* And switching back to Zigbee must not leave it below 11. */
  ch = rf_channel_clamp(RF_PROTOCOL_ZIGBEE, 3);
  TEST_ASSERT_EQUAL_UINT8(RF_ZIGBEE_MIN_CHANNEL, ch);
}

static void test_wifi_frequency_calculation(void) {
  TEST_ASSERT_EQUAL_UINT16(2412, rf_channel_to_frequency_mhz(RF_PROTOCOL_WIFI, 1));
  TEST_ASSERT_EQUAL_UINT16(2437, rf_channel_to_frequency_mhz(RF_PROTOCOL_WIFI, 6));
  TEST_ASSERT_EQUAL_UINT16(2462, rf_channel_to_frequency_mhz(RF_PROTOCOL_WIFI, 11));
  TEST_ASSERT_EQUAL_UINT16(2472, rf_channel_to_frequency_mhz(RF_PROTOCOL_WIFI, 13));
  TEST_ASSERT_EQUAL_UINT16(2484, rf_channel_to_frequency_mhz(RF_PROTOCOL_WIFI, 14));
}

static void test_zigbee_frequency_calculation(void) {
  TEST_ASSERT_EQUAL_UINT16(2405, rf_channel_to_frequency_mhz(RF_PROTOCOL_ZIGBEE, 11));
  TEST_ASSERT_EQUAL_UINT16(2425, rf_channel_to_frequency_mhz(RF_PROTOCOL_ZIGBEE, 15));
  TEST_ASSERT_EQUAL_UINT16(2450, rf_channel_to_frequency_mhz(RF_PROTOCOL_ZIGBEE, 20));
  TEST_ASSERT_EQUAL_UINT16(2480, rf_channel_to_frequency_mhz(RF_PROTOCOL_ZIGBEE, 26));
}

/* --- Filter & Jammer Tests --------------------------------------------- */

static void test_next_sweep_linear_and_tri_channel(void) {
  /* Wi-Fi standard linear sweep */
  TEST_ASSERT_EQUAL_UINT8(2, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 1, false));
  TEST_ASSERT_EQUAL_UINT8(13, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 12, false));
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 13, false));

  /* Wi-Fi tri-channel mode (1, 6, 11) */
  TEST_ASSERT_EQUAL_UINT8(6, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 1, true));
  TEST_ASSERT_EQUAL_UINT8(11, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 6, true));
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 11, true));
  TEST_ASSERT_EQUAL_UINT8(1, rf_channel_next_sweep(RF_PROTOCOL_WIFI, 4, true));

  /* Zigbee sweep */
  TEST_ASSERT_EQUAL_UINT8(12, rf_channel_next_sweep(RF_PROTOCOL_ZIGBEE, 11, false));
  TEST_ASSERT_EQUAL_UINT8(26, rf_channel_next_sweep(RF_PROTOCOL_ZIGBEE, 25, false));
  TEST_ASSERT_EQUAL_UINT8(11, rf_channel_next_sweep(RF_PROTOCOL_ZIGBEE, 26, false));
}

static void test_filter_ema_rssi(void) {
  /* Reset on noise floor */
  TEST_ASSERT_EQUAL_INT8(-50, rf_filter_ema_rssi(RF_NOISE_FLOOR, -50, 30));

  /* Steady state */
  TEST_ASSERT_EQUAL_INT8(-50, rf_filter_ema_rssi(-50, -50, 30));

  /* Smooth transition: 30% of -40 + 70% of -50 = -12 + -35 = -47 */
  TEST_ASSERT_EQUAL_INT8(-47, rf_filter_ema_rssi(-50, -40, 30));

  /* Ignore noise floor samples when already tracking */
  TEST_ASSERT_EQUAL_INT8(-50, rf_filter_ema_rssi(-50, RF_NOISE_FLOOR, 30));
}

static void test_jammer_evaluator_discriminates_legitimate_ap(void) {
  /* Legitimate AP with strong RSSI (-35 dBm) must NEVER trigger jammer alert */
  TEST_ASSERT_FALSE(rf_jammer_evaluate(-35, 10, 0, 0, 50, false));

  /* Legitimate AP traffic with some data frames and beacons */
  TEST_ASSERT_FALSE(rf_jammer_evaluate(-40, 2, 0, 1, 20, false));
}

static void test_jammer_evaluator_discriminates_adjacent_spillover(void) {
  /* Adjacent channel hearing spillover energy without beacons or errors */
  TEST_ASSERT_FALSE(rf_jammer_evaluate(-48, 0, 0, 0, 5, true));
}

static void test_jammer_evaluator_detects_deauth_flooder(void) {
  /* Deauth / Disassociation flood attack */
  TEST_ASSERT_TRUE(rf_jammer_evaluate(-60, 0, 15, 0, 0, false));
}

static void test_jammer_evaluator_detects_rf_noise_jammer(void) {
  /* Continuous high energy with severe FCS errors and no valid beacons */
  TEST_ASSERT_TRUE(rf_jammer_evaluate(-38, 0, 0, 25, 0, false));

  /* Weak background noise should not trigger */
  TEST_ASSERT_FALSE(rf_jammer_evaluate(-80, 0, 0, 5, 0, false));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_storage_covers_the_widest_channel_plan);
  RUN_TEST(test_indexable_range_is_exactly_one_to_max);
  RUN_TEST(test_wifi_bounds);
  RUN_TEST(test_zigbee_bounds);
  RUN_TEST(test_unknown_protocol_falls_back_to_the_wifi_plan);
  RUN_TEST(test_clamp_leaves_valid_channels_alone);
  RUN_TEST(test_clamp_pulls_out_of_range_channels_back_in);
  RUN_TEST(test_clamp_output_is_always_indexable);
  RUN_TEST(test_switching_to_zigbee_does_not_select_an_unindexable_channel);
  RUN_TEST(test_tuner_cannot_walk_out_of_the_band);
  RUN_TEST(test_switching_protocols_reclaims_the_channel);
  RUN_TEST(test_wifi_frequency_calculation);
  RUN_TEST(test_zigbee_frequency_calculation);
  RUN_TEST(test_next_sweep_linear_and_tri_channel);
  RUN_TEST(test_filter_ema_rssi);
  RUN_TEST(test_jammer_evaluator_discriminates_legitimate_ap);
  RUN_TEST(test_jammer_evaluator_discriminates_adjacent_spillover);
  RUN_TEST(test_jammer_evaluator_detects_deauth_flooder);
  RUN_TEST(test_jammer_evaluator_detects_rf_noise_jammer);
  return UNITY_END();
}