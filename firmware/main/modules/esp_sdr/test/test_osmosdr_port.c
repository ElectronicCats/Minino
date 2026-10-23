/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Unit tests for Osmosdr serial protocol parser and I/Q packing on ESP-IDF host.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"

#include "esdr_proto.h"
#include "osmosdr_serial.h"
#include "iq_pack.h"

static osmosdr_serial_ctx_t ctx;
static bool escape_cb_called = false;

static void dummy_escape_cb(void *arg) {
    bool *flag = (bool *)arg;
    if (flag != NULL) {
        *flag = true;
    }
}

void setUp(void) {
    osmosdr_serial_init(&ctx);
    escape_cb_called = false;
}

void tearDown(void) {
}

/* --------------------------------------------------------------------------
 * Osmosdr / HackRF GET_INFO tests
 * -------------------------------------------------------------------------- */

void test_cmd_get_info_binary_request(void) {
    uint8_t req_buf[128];
    size_t req_len = osmosdr_build_request_frame(ESDR_REQ_GET_INFO, ESDR_SER_IN, 0, NULL,
                                                 sizeof(struct esdr_info), req_buf, sizeof(req_buf));
    TEST_ASSERT_GREATER_THAN(0, req_len);

    size_t fed = osmosdr_serial_feed_bytes(&ctx, req_buf, req_len);
    TEST_ASSERT_EQUAL(1, fed);
    TEST_ASSERT_TRUE(ctx.frame_ready);

    int ret = osmosdr_serial_dispatch(&ctx);
    TEST_ASSERT_EQUAL((int)sizeof(struct esdr_info), ret);
    TEST_ASSERT_EQUAL(sizeof(struct esdr_ser_hdr) + sizeof(struct esdr_info), ctx.out_len);

    const struct esdr_ser_hdr *rep_hdr = (const struct esdr_ser_hdr *)ctx.out_buf;
    TEST_ASSERT_EQUAL_HEX32(ESDR_SER_MAGIC, rep_hdr->magic);
    TEST_ASSERT_EQUAL_UINT8(ESDR_REQ_GET_INFO, rep_hdr->req);
    TEST_ASSERT_EQUAL_INT16(0, rep_hdr->status);

    const struct esdr_info *info = (const struct esdr_info *)(ctx.out_buf + sizeof(struct esdr_ser_hdr));
    TEST_ASSERT_EQUAL_HEX32(ESDR_INFO_MAGIC, info->magic);
    TEST_ASSERT_EQUAL_UINT16(ESDR_PROTO_VERSION, info->proto_version);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)ESP_SDR_FREQ_MIN_MHZ * 1000000ULL, info->freq_min_hz);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)ESP_SDR_FREQ_MAX_MHZ * 1000000ULL, info->freq_max_hz);
    TEST_ASSERT_EQUAL_UINT32(ESDR_RATE_MAX_HZ, info->rx_rate_max_hz);
    TEST_ASSERT_EQUAL_UINT16(ESDR_RATE_COUNT, info->rx_rate_count);
    TEST_ASSERT_EQUAL_UINT16(ESP_SDR_RX_GAIN_MAX, info->rx_gain_max);
    TEST_ASSERT_EQUAL_UINT16(ESP_SDR_TX_GAIN_MAX, info->tx_gain_max);
    TEST_ASSERT_EQUAL_UINT16(ESDR_BLOCK_BYTES, info->block_bytes);
    TEST_ASSERT_EQUAL_UINT16(32767, info->full_scale16);
    TEST_ASSERT_NOT_NULL(strstr(info->firmware, "osmosdr_serial"));
}

void test_cmd_get_info_invalid_params(void) {
    uint8_t buf[64];
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_in(&ctx, ESDR_REQ_GET_INFO, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_in(&ctx, ESDR_REQ_GET_INFO, buf, 0));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_in(NULL, ESDR_REQ_GET_INFO, buf, sizeof(buf)));
}

/* --------------------------------------------------------------------------
 * SET_FREQ tests: validation, clamping, and rejection
 * -------------------------------------------------------------------------- */

void test_cmd_set_freq_valid(void) {
    uint8_t req_buf[64];
    uint8_t payload[8];

    /* 2400 MHz */
    uint64_t freq_hz = 2400000000ULL;
    memcpy(payload, &freq_hz, sizeof(freq_hz));
    size_t req_len = osmosdr_build_request_frame(ESDR_REQ_SET_FREQ, 0, 0, payload, sizeof(payload),
                                                 req_buf, sizeof(req_buf));
    osmosdr_serial_feed_bytes(&ctx, req_buf, req_len);
    TEST_ASSERT_EQUAL(0, osmosdr_serial_dispatch(&ctx));
    TEST_ASSERT_EQUAL_UINT64(freq_hz, ctx.settings.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(2400, ctx.settings.lo_mhz);
    TEST_ASSERT_EQUAL_INT32(0, ctx.settings.lo_offset_khz);

    /* 2450.5 MHz (testing kHz offset calculation) */
    freq_hz = 2450500000ULL;
    memcpy(payload, &freq_hz, sizeof(freq_hz));
    req_len = osmosdr_build_request_frame(ESDR_REQ_SET_FREQ, 0, 0, payload, sizeof(payload),
                                          req_buf, sizeof(req_buf));
    osmosdr_serial_feed_bytes(&ctx, req_buf, req_len);
    TEST_ASSERT_EQUAL(0, osmosdr_serial_dispatch(&ctx));
    TEST_ASSERT_EQUAL_UINT64(freq_hz, ctx.settings.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(2450, ctx.settings.lo_mhz);
    TEST_ASSERT_EQUAL_INT32(500, ctx.settings.lo_offset_khz);
}

void test_cmd_set_freq_clamping_and_rejection(void) {
    uint8_t payload[8];

    /* Below minimum (99 MHz < 100 MHz): must be rejected */
    uint64_t freq_too_low = 99000000ULL;
    memcpy(payload, &freq_too_low, sizeof(freq_too_low));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, sizeof(payload)));

    /* Above maximum (6001 MHz > 6000 MHz): must be rejected */
    uint64_t freq_too_high = 6001000000ULL;
    memcpy(payload, &freq_too_high, sizeof(freq_too_high));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, sizeof(payload)));

    /* Zero frequency: rejected */
    uint64_t freq_zero = 0ULL;
    memcpy(payload, &freq_zero, sizeof(freq_zero));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, sizeof(payload)));

    /* Invalid length or NULL */
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, 4));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, NULL, sizeof(payload)));

    /* Boundary frequencies: exactly MIN and MAX are accepted */
    uint64_t freq_min = (uint64_t)ESP_SDR_FREQ_MIN_MHZ * 1000000ULL;
    memcpy(payload, &freq_min, sizeof(freq_min));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(ESP_SDR_FREQ_MIN_MHZ, ctx.settings.lo_mhz);

    uint64_t freq_max = (uint64_t)ESP_SDR_FREQ_MAX_MHZ * 1000000ULL;
    memcpy(payload, &freq_max, sizeof(freq_max));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FREQ, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(ESP_SDR_FREQ_MAX_MHZ, ctx.settings.lo_mhz);
}

/* --------------------------------------------------------------------------
 * SET_SAMPLE_RATE tests: valid, nearest rounding, zero rejection
 * -------------------------------------------------------------------------- */

void test_cmd_set_sample_rate_valid_and_rounding(void) {
    uint8_t payload[4];

    /* Exact 250 kS/s */
    uint32_t rate = 250000U;
    memcpy(payload, &rate, sizeof(rate));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(250000U, ctx.settings.rate_hz);

    /* Exact 125 kS/s */
    rate = 125000U;
    memcpy(payload, &rate, sizeof(rate));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(125000U, ctx.settings.rate_hz);

    /* Rounding: 200000 rounds up to 250000 */
    rate = 200000U;
    memcpy(payload, &rate, sizeof(rate));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(250000U, ctx.settings.rate_hz);

    /* Rounding: 70000 rounds down to 62500 */
    rate = 70000U;
    memcpy(payload, &rate, sizeof(rate));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(62500U, ctx.settings.rate_hz);
}

void test_cmd_set_sample_rate_zero_rejected(void) {
    uint8_t payload[4];

    /* Sample rate 0 must return -EINVAL */
    uint32_t zero_rate = 0U;
    memcpy(payload, &zero_rate, sizeof(zero_rate));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, sizeof(payload)));

    /* Truncated payload or NULL must return -EINVAL */
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, payload, 2));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_SAMPLE_RATE, 0, NULL, sizeof(payload)));
}

/* --------------------------------------------------------------------------
 * SET_RX_GAIN tests: AUTO, limits, and out of range rejection
 * -------------------------------------------------------------------------- */

void test_cmd_set_rx_gain_valid(void) {
    /* AUTO gain */
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, ESDR_GAIN_AUTO, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(ESDR_GAIN_AUTO, ctx.settings.rx_gain);

    /* Zero gain */
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, 0, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(0, ctx.settings.rx_gain);

    /* Mid gain */
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, 24, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(24, ctx.settings.rx_gain);

    /* Maximum gain */
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, ESP_SDR_RX_GAIN_MAX, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(ESP_SDR_RX_GAIN_MAX, ctx.settings.rx_gain);
}

void test_cmd_set_rx_gain_out_of_range_rejected(void) {
    /* Max + 1 must be rejected */
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, ESP_SDR_RX_GAIN_MAX + 1, NULL, 0));

    /* Very large value */
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_RX_GAIN, 1000, NULL, 0));
}

/* --------------------------------------------------------------------------
 * SET_BANDWIDTH tests
 * -------------------------------------------------------------------------- */

void test_cmd_set_bandwidth(void) {
    uint8_t payload[4];

    /* 0 = auto */
    uint32_t bw = 0U;
    memcpy(payload, &bw, sizeof(bw));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_BANDWIDTH, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(0U, ctx.settings.bandwidth_hz);

    /* 20 MHz */
    bw = 20000000U;
    memcpy(payload, &bw, sizeof(bw));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_BANDWIDTH, 0, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(20000000U, ctx.settings.bandwidth_hz);

    /* Invalid length */
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_BANDWIDTH, 0, payload, 2));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_BANDWIDTH, 0, NULL, sizeof(payload)));
}

/* --------------------------------------------------------------------------
 * Other commands: SET_MODE, SET_FORMAT, GET_STATE, GET_STATS, UNSUPPORTED
 * -------------------------------------------------------------------------- */

void test_cmd_set_mode_and_format(void) {
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_MODE, ESDR_MODE_OFF, NULL, 0));
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_MODE, ESDR_MODE_RX, NULL, 0));
    /* The C6 has no transmitter: the host is told the mode is not
       supported instead of being answered with silence. */
    TEST_ASSERT_EQUAL(-ENOTSUP, osmosdr_handle_out(&ctx, ESDR_REQ_SET_MODE, ESDR_MODE_TX, NULL, 0));
    TEST_ASSERT_EQUAL(-ENOTSUP, osmosdr_handle_out(&ctx, ESDR_REQ_SET_MODE, 0xFF, NULL, 0));

    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FORMAT, 8, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(8, ctx.settings.bits);
    TEST_ASSERT_EQUAL(0, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FORMAT, 16, NULL, 0));
    TEST_ASSERT_EQUAL_UINT16(16, ctx.settings.bits);
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FORMAT, 10, NULL, 0));
    TEST_ASSERT_EQUAL(-EINVAL, osmosdr_handle_out(&ctx, ESDR_REQ_SET_FORMAT, 32, NULL, 0));

    /* Unsupported requests */
    TEST_ASSERT_EQUAL(-ENOTSUP, osmosdr_handle_out(&ctx, 0xFE, 0, NULL, 0));
    uint8_t dummy[16];
    TEST_ASSERT_EQUAL(-ENOTSUP, osmosdr_handle_in(&ctx, 0xFE, dummy, sizeof(dummy)));
}

void test_cmd_get_state_and_stats(void) {
    struct esdr_state st;
    int ret = osmosdr_handle_in(&ctx, ESDR_REQ_GET_STATE, (uint8_t *)&st, sizeof(st));
    TEST_ASSERT_EQUAL((int)sizeof(st), ret);
    TEST_ASSERT_EQUAL_UINT64(ctx.settings.freq_hz, st.freq_hz);
    TEST_ASSERT_EQUAL_UINT32(ctx.settings.rate_hz, st.sample_rate_hz);

    struct esdr_stats stats;
    ctx.stats.rx_samples = 123456ULL;
    ret = osmosdr_handle_in(&ctx, ESDR_REQ_GET_STATS, (uint8_t *)&stats, sizeof(stats));
    TEST_ASSERT_EQUAL((int)sizeof(stats), ret);
    TEST_ASSERT_EQUAL_UINT64(123456ULL, stats.rx_samples);
}

/* --------------------------------------------------------------------------
 * MININO escape command tests
 * -------------------------------------------------------------------------- */

void test_minino_escape_ascii_newline(void) {
    osmosdr_serial_set_escape_cb(&ctx, dummy_escape_cb, &escape_cb_called);
    const char *cmd = "MININO\n";
    size_t triggered = osmosdr_serial_feed_bytes(&ctx, (const uint8_t *)cmd, strlen(cmd));
    TEST_ASSERT_GREATER_THAN(0, triggered);
    TEST_ASSERT_TRUE(osmosdr_serial_is_minino_escape(&ctx));
    TEST_ASSERT_TRUE(escape_cb_called);
}

void test_minino_escape_ascii_crlf(void) {
    osmosdr_serial_set_escape_cb(&ctx, dummy_escape_cb, &escape_cb_called);
    const char *cmd = "MININO\r\n";
    osmosdr_serial_feed_bytes(&ctx, (const uint8_t *)cmd, strlen(cmd));
    TEST_ASSERT_TRUE(osmosdr_serial_is_minino_escape(&ctx));
    TEST_ASSERT_TRUE(escape_cb_called);
}

void test_minino_escape_ascii_raw_sequence(void) {
    osmosdr_serial_set_escape_cb(&ctx, dummy_escape_cb, &escape_cb_called);
    const char *cmd = "MININO";
    osmosdr_serial_feed_bytes(&ctx, (const uint8_t *)cmd, strlen(cmd));
    TEST_ASSERT_TRUE(osmosdr_serial_is_minino_escape(&ctx));
    TEST_ASSERT_TRUE(escape_cb_called);
}

void test_minino_escape_binary_request(void) {
    osmosdr_serial_set_escape_cb(&ctx, dummy_escape_cb, &escape_cb_called);
    uint8_t req_buf[32];
    size_t req_len = osmosdr_build_request_frame(ESDR_REQ_MININO, 0, 0, NULL, 0, req_buf, sizeof(req_buf));
    osmosdr_serial_feed_bytes(&ctx, req_buf, req_len);
    TEST_ASSERT_TRUE(ctx.frame_ready);
    int ret = osmosdr_serial_dispatch(&ctx);
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_TRUE(osmosdr_serial_is_minino_escape(&ctx));
    TEST_ASSERT_TRUE(escape_cb_called);
}

void test_minino_escape_noise_immunity_and_clear(void) {
    osmosdr_serial_set_escape_cb(&ctx, dummy_escape_cb, &escape_cb_called);
    const char *noise = "MINI_NOT_CMD_MININX_1234";
    osmosdr_serial_feed_bytes(&ctx, (const uint8_t *)noise, strlen(noise));
    TEST_ASSERT_FALSE(osmosdr_serial_is_minino_escape(&ctx));
    TEST_ASSERT_FALSE(escape_cb_called);

    /* Now send valid MININO and verify clear */
    const char *cmd = "MININO\n";
    osmosdr_serial_feed_bytes(&ctx, (const uint8_t *)cmd, strlen(cmd));
    TEST_ASSERT_TRUE(osmosdr_serial_is_minino_escape(&ctx));

    osmosdr_serial_clear_minino_escape(&ctx);
    TEST_ASSERT_FALSE(osmosdr_serial_is_minino_escape(&ctx));
}

void test_serial_resync_on_garbage_bytes(void) {
    uint8_t stream[128];
    /* Prepend 8 bytes of garbage */
    for (int i = 0; i < 8; i++) {
        stream[i] = (uint8_t)(0xAA + i);
    }
    /* Follow with valid GET_INFO frame */
    size_t req_len = osmosdr_build_request_frame(ESDR_REQ_GET_INFO, ESDR_SER_IN, 0, NULL,
                                                 sizeof(struct esdr_info), stream + 8, sizeof(stream) - 8);
    osmosdr_serial_feed_bytes(&ctx, stream, 8 + req_len);
    TEST_ASSERT_TRUE(ctx.frame_ready);
    int ret = osmosdr_serial_dispatch(&ctx);
    TEST_ASSERT_EQUAL((int)sizeof(struct esdr_info), ret);
}

/* --------------------------------------------------------------------------
 * I/Q Sample Packing and Unpacking tests (8, 12, 16 bits)
 * -------------------------------------------------------------------------- */

void test_iq_pack_16bit_is_lossless(void) {
    static const int16_t values[] = {
        0, 1, -1, 100, -100, 32767, -32768, 12345, -12345, 512, -512
    };
    size_t count = sizeof(values) / sizeof(values[0]);

    for (size_t k = 0; k < count; k++) {
        struct esp_sdr_iq16 s[1] = {{.i = values[k], .q = (int16_t)(values[k] ^ 0x5a5a)}};
        uint32_t word = 0;
        int16_t out_iq[2] = {0};

        TEST_ASSERT_EQUAL(1, iq_pack(s, 1, 16, 32768, &word));
        TEST_ASSERT_EQUAL(1, iq_unpack(&word, 1, 16, out_iq, 1));
        TEST_ASSERT_EQUAL_INT16(s[0].i, out_iq[0]);
        TEST_ASSERT_EQUAL_INT16(s[0].q, out_iq[1]);
    }
}

void test_iq_pack_8bit_lossless_roundtrip(void) {
    /* Test all 256 signed 8-bit values without loss using raw pack/unpack */
    for (int val = -128; val <= 127; val += 2) {
        struct esp_sdr_iq16 s[2];
        s[0].i = (int16_t)val;
        s[0].q = (int16_t)(val == -128 ? 127 : -val);
        int val2 = (val + 1 <= 127) ? (val + 1) : 127;
        s[1].i = (int16_t)val2;
        s[1].q = (int16_t)(val2 == -128 ? 127 : -val2);

        uint32_t words[2];
        struct esp_sdr_iq16 recovered[2];

        size_t w = iq_pack_raw(s, 2, 8, words);
        TEST_ASSERT_EQUAL(1, w);
        size_t n = iq_unpack_raw(words, w, 8, recovered, 2);
        TEST_ASSERT_EQUAL(2, n);

        TEST_ASSERT_EQUAL_INT16(s[0].i, recovered[0].i);
        TEST_ASSERT_EQUAL_INT16(s[0].q, recovered[0].q);
        TEST_ASSERT_EQUAL_INT16(s[1].i, recovered[1].i);
        TEST_ASSERT_EQUAL_INT16(s[1].q, recovered[1].q);
    }
}

void test_iq_pack_12bit_lossless_roundtrip(void) {
    /* Test representative 12-bit signed values (-2048 to 2047) */
    static const int16_t vals_12bit[] = {
        -2048, -1500, -1000, -512, -1, 0, 1, 512, 1000, 1500, 2047, -2047
    };
    size_t count = sizeof(vals_12bit) / sizeof(vals_12bit[0]);

    for (size_t k = 0; k + 4 <= count; k += 4) {
        struct esp_sdr_iq16 s[4];
        for (int m = 0; m < 4; m++) {
            s[m].i = vals_12bit[k + m];
            s[m].q = (int16_t)(vals_12bit[k + m] == -2048 ? 2047 : -vals_12bit[k + m]);
        }

        uint32_t words[4];
        struct esp_sdr_iq16 recovered[4];

        size_t w = iq_pack_raw(s, 4, 12, words);
        TEST_ASSERT_EQUAL(3, w); /* 4 samples of 12-bit I/Q = 8 items * 12 = 96 bits = 3 words */
        size_t n = iq_unpack_raw(words, w, 12, recovered, 4);
        TEST_ASSERT_EQUAL(4, n);

        for (int m = 0; m < 4; m++) {
            TEST_ASSERT_EQUAL_INT16(s[m].i, recovered[m].i);
            TEST_ASSERT_EQUAL_INT16(s[m].q, recovered[m].q);
        }
    }
}

void test_iq_pack_zero_maps_to_zero(void) {
    static const unsigned int bit_opts[] = {8, 12, 16};
    static const int32_t scales[] = {512, 32768};

    for (size_t b = 0; b < 3; b++) {
        for (size_t f = 0; f < 2; f++) {
            struct esp_sdr_iq16 s[4] = {{.i = 0, .q = 0}};
            uint32_t words[8];
            int16_t iq[8];

            size_t n = iq_sample_quantum(bit_opts[b]);
            size_t w = iq_pack(s, n, bit_opts[b], scales[f], words);
            size_t got = iq_unpack(words, w, bit_opts[b], iq, n);

            TEST_ASSERT_EQUAL(n, got);
            TEST_ASSERT_EQUAL_INT16(0, iq[0]);
            TEST_ASSERT_EQUAL_INT16(0, iq[1]);
        }
    }
}

void test_iq_pack_quanta_and_budget(void) {
    TEST_ASSERT_EQUAL(2, iq_sample_quantum(8));
    TEST_ASSERT_EQUAL(4, iq_sample_quantum(12));
    TEST_ASSERT_EQUAL(1, iq_sample_quantum(16));

    static const unsigned int bit_opts[] = {8, 12, 16};
    for (size_t b = 0; b < 3; b++) {
        unsigned int bits = bit_opts[b];
        size_t quantum = iq_sample_quantum(bits);
        size_t max_words = 128;
        size_t n = iq_samples_per_words(bits, max_words);

        TEST_ASSERT_EQUAL(0, n % quantum);
        TEST_ASSERT_TRUE(n * 2U * bits <= max_words * 32U);
        TEST_ASSERT_TRUE((n + quantum) * 2U * bits > max_words * 32U);
    }
}

void test_iq_pack_partial_tail(void) {
    static const unsigned int bit_opts[] = {8, 12};

    for (size_t b = 0; b < 2; b++) {
        unsigned int bits = bit_opts[b];
        size_t quantum = iq_sample_quantum(bits);

        for (size_t n = 1; n <= 2U * quantum + 1U; n++) {
            struct esp_sdr_iq16 s[10];
            uint32_t words[16];
            int16_t iq[20];

            for (size_t k = 0; k < n; k++) {
                s[k].i = (int16_t)(16 * (int)k - 64);
                s[k].q = (int16_t)(-8 * (int)k);
            }

            size_t w = iq_pack(s, n, bits, 512, words);
            size_t got = iq_unpack(words, w, bits, iq, n);
            TEST_ASSERT_EQUAL(n, got);

            TEST_ASSERT_TRUE(iq[0] < 0);
            for (size_t k = 1; k < n; k++) {
                TEST_ASSERT_TRUE(iq[2 * k] > iq[2 * (k - 1)]);
                TEST_ASSERT_TRUE(iq[2 * k + 1] < iq[2 * k - 1]);
            }
        }
    }
}

/* --------------------------------------------------------------------------
 * Main Runner
 * -------------------------------------------------------------------------- */

int main(void) {
    UNITY_BEGIN();

    /* 1. GET_INFO binary command */
    RUN_TEST(test_cmd_get_info_binary_request);
    RUN_TEST(test_cmd_get_info_invalid_params);

    /* 2. SET_FREQ binary command, clamping, and rejection */
    RUN_TEST(test_cmd_set_freq_valid);
    RUN_TEST(test_cmd_set_freq_clamping_and_rejection);

    /* 3. SET_SAMPLE_RATE binary command, rounding, zero rejection */
    RUN_TEST(test_cmd_set_sample_rate_valid_and_rounding);
    RUN_TEST(test_cmd_set_sample_rate_zero_rejected);

    /* 4. SET_RX_GAIN binary command, limits, and rejection */
    RUN_TEST(test_cmd_set_rx_gain_valid);
    RUN_TEST(test_cmd_set_rx_gain_out_of_range_rejected);

    /* 5. SET_BANDWIDTH binary command */
    RUN_TEST(test_cmd_set_bandwidth);

    /* Other binary commands */
    RUN_TEST(test_cmd_set_mode_and_format);
    RUN_TEST(test_cmd_get_state_and_stats);

    /* 6. MININO escape handling */
    RUN_TEST(test_minino_escape_ascii_newline);
    RUN_TEST(test_minino_escape_ascii_crlf);
    RUN_TEST(test_minino_escape_ascii_raw_sequence);
    RUN_TEST(test_minino_escape_binary_request);
    RUN_TEST(test_minino_escape_noise_immunity_and_clear);
    RUN_TEST(test_serial_resync_on_garbage_bytes);

    /* 7. I/Q sample packing / unpacking (8, 12, 16 bits without loss) */
    RUN_TEST(test_iq_pack_16bit_is_lossless);
    RUN_TEST(test_iq_pack_8bit_lossless_roundtrip);
    RUN_TEST(test_iq_pack_12bit_lossless_roundtrip);
    RUN_TEST(test_iq_pack_zero_maps_to_zero);
    RUN_TEST(test_iq_pack_quanta_and_budget);
    RUN_TEST(test_iq_pack_partial_tail);

    return UNITY_END();
}
