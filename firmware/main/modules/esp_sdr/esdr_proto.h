/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Osmosdr protocol definitions ported to ESP-IDF for Minino.
 */

#ifndef ESDR_PROTO_H_
#define ESDR_PROTO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define ESDR_PROTO_VERSION 1U

/* Vendor requests (bRequest) */
enum esdr_req {
    ESDR_REQ_GET_INFO         = 0x01,
    ESDR_REQ_SET_MODE         = 0x02,
    ESDR_REQ_SET_FREQ         = 0x03,
    ESDR_REQ_SET_SAMPLE_RATE  = 0x04,
    ESDR_REQ_SET_RX_GAIN      = 0x05,
    ESDR_REQ_SET_TX_GAIN      = 0x06,
    ESDR_REQ_SET_BANDWIDTH    = 0x07,
    ESDR_REQ_SET_FORMAT       = 0x08,
    ESDR_REQ_SET_FREQ_CORR    = 0x09,
    ESDR_REQ_GET_STATE        = 0x0a,
    ESDR_REQ_GET_STATS        = 0x0b,
    ESDR_REQ_SET_OPTIONS      = 0x0c,
    ESDR_REQ_SET_DIGITAL_GAIN = 0x0d,
    ESDR_REQ_MININO           = 0x7e, /* Escape command request */
};

enum esdr_mode {
    ESDR_MODE_OFF      = 0,
    ESDR_MODE_RX       = 1,
    ESDR_MODE_TX       = 2,
    ESDR_MODE_TEST_IN  = 0x10,
    ESDR_MODE_TEST_OUT = 0x11,
};

#define ESDR_GAIN_AUTO      0xffffU
#define ESDR_DGAIN_DEFAULT  0xffffU
#define ESDR_DGAIN_MAX_DB   42U

#define ESDR_OPT_OFFSET_TUNING 0x0001U

#define ESDR_INFO_MAGIC  0x52445345U /* "ESDR" */
#define ESDR_BLOCK_MAGIC 0x4b4c4245U /* "EBLK" */
#define ESDR_BLOCK_BYTES 4096U

#define ESDR_SER_MAGIC    0x4c525345U /* "ESRL" */
#define ESDR_SER_IN       0x01U
#define ESDR_SER_TX_DATA  0x80U
#define ESDR_SER_MAX_DATA 512U

#define ESP_SDR_FREQ_MIN_MHZ 100U
#define ESP_SDR_FREQ_MAX_MHZ 6000U
#define ESP_SDR_RX_GAIN_MAX  48U
#define ESP_SDR_TX_GAIN_MAX  15U

#define ESDR_RATE_MAX_HZ 250000U
#define ESDR_RATE_COUNT  5U

#pragma pack(push, 1)
struct esdr_ser_hdr {
    uint32_t magic;
    uint8_t  req;
    uint8_t  flags;
    uint16_t value;
    uint16_t len;
    int16_t  status;
};

struct esdr_info {
    uint32_t magic;
    uint16_t proto_version;
    uint16_t info_size;
    char     firmware[32];
    uint64_t freq_min_hz;
    uint64_t freq_max_hz;
    uint32_t rx_rate_max_hz;
    uint16_t rx_rate_count;
    uint16_t rx_gain_max;
    uint16_t tx_gain_max;
    uint16_t block_bytes;
    uint16_t full_scale16;
    uint16_t reserved;
};

struct esdr_state {
    uint16_t mode;
    uint16_t bits;
    uint16_t options;
    uint16_t rx_gain;
    uint16_t tx_gain;
    uint16_t digital_gain_db;
    uint64_t freq_hz;
    uint64_t center_hz;
    uint32_t sample_rate_hz;
    uint32_t bandwidth_hz;
    int32_t  freq_corr_ppb;
    uint32_t lo_mhz;
    int32_t  lo_offset_khz;
};

struct esdr_stats {
    uint64_t rx_samples;
    uint64_t rx_overflow_samples;
    uint64_t rx_ring_lost_pairs;
    uint64_t rx_restart_samples;
    uint64_t usb_in_bytes;
    uint64_t usb_out_bytes;
    uint32_t rx_blocks;
    uint32_t rx_overflows;
    uint32_t ring_runs;
    uint32_t ring_errors;
    uint32_t ring_late_max;
    uint32_t ring_cycles_x100;
    uint32_t ring_abandoned;
    uint32_t ring_gain_refreshed;
    uint32_t ring_last_status;
    uint32_t ring_last_detail;
    uint64_t tx_samples;
    uint64_t tx_dropped;
    uint32_t tx_underruns;
    uint32_t tx_sessions;
    uint32_t tx_restarts;
    uint32_t tx_queued_max;
};
#pragma pack(pop)

#endif /* ESDR_PROTO_H_ */
