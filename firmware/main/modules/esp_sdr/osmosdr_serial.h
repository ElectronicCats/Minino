/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Osmosdr serial protocol parser and command handler ported to ESP-IDF.
 */

#ifndef OSMOSDR_SERIAL_H_
#define OSMOSDR_SERIAL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esdr_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t mode;
    uint16_t bits;
    uint16_t rx_gain;
    uint16_t tx_gain;
    uint16_t dgain;
    uint64_t freq_hz;
    uint32_t rate_hz;
    uint32_t bandwidth_hz;
    int32_t  corr_ppb;
    uint32_t lo_mhz;
    int32_t  lo_offset_khz;
    uint64_t center_hz;
    bool     radio_dirty;
} osmosdr_settings_t;

typedef void (*osmosdr_escape_cb_t)(void *arg);

typedef struct {
    osmosdr_settings_t settings;
    struct esdr_stats  stats;

    /* Frame reception */
    union {
        struct esdr_ser_hdr hdr;
        uint8_t raw_hdr[sizeof(struct esdr_ser_hdr)];
    };
    uint8_t  data[ESDR_SER_MAX_DATA];
    uint32_t got;
    uint32_t tx_left;
    bool     frame_ready;

    /* Escape detection ("MININO") */
    uint8_t             escape_match_idx;
    bool                minino_escape_triggered;
    osmosdr_escape_cb_t escape_cb;
    void               *escape_cb_arg;

    /* Outgoing response frame */
    uint8_t out_buf[sizeof(struct esdr_ser_hdr) + ESDR_SER_MAX_DATA];
    size_t  out_len;
} osmosdr_serial_ctx_t;

/**
 * Initialize context with default parameters.
 */
void osmosdr_serial_init(osmosdr_serial_ctx_t *ctx);

/**
 * Register callback to be called when MININO escape is received.
 */
void osmosdr_serial_set_escape_cb(osmosdr_serial_ctx_t *ctx, osmosdr_escape_cb_t cb, void *arg);

/**
 * Round sample rate to nearest supported rate (250k, 125k, 62.5k, 31.25k, 15.625k).
 */
uint32_t osmosdr_rate_round(uint32_t hz);

/**
 * Direct command handlers (OUT and IN).
 */
int osmosdr_handle_out(osmosdr_serial_ctx_t *ctx, uint8_t req, uint16_t value, const uint8_t *data,
                       size_t len);
int osmosdr_handle_in(osmosdr_serial_ctx_t *ctx, uint8_t req, uint8_t *buf, size_t max);

/**
 * Feed bytes into the serial state machine.
 * Returns true if a complete frame was received or MININO escape occurred.
 */
bool osmosdr_serial_feed_byte(osmosdr_serial_ctx_t *ctx, uint8_t b);
size_t osmosdr_serial_feed_bytes(osmosdr_serial_ctx_t *ctx, const uint8_t *buf, size_t len);

/**
 * Dispatch received frame and prepare response in ctx->out_buf.
 * Returns 0 on success or negative errno.
 */
int osmosdr_serial_dispatch(osmosdr_serial_ctx_t *ctx);

/**
 * Check and clear MININO escape flag.
 */
bool osmosdr_serial_is_minino_escape(const osmosdr_serial_ctx_t *ctx);
void osmosdr_serial_clear_minino_escape(osmosdr_serial_ctx_t *ctx);

/**
 * Helper to build a binary request frame for host/test.
 * Returns total frame size in bytes.
 */
size_t osmosdr_build_request_frame(uint8_t req, uint8_t flags, uint16_t value,
                                   const uint8_t *data, uint16_t len,
                                   uint8_t *out_buf, size_t max_out);


#ifdef __cplusplus
}
#endif

#endif /* OSMOSDR_SERIAL_H_ */
