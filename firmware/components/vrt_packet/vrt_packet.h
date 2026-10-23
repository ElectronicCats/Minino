/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * VITA 49.2 Radio Transport (VRT) frame structures, header constructors,
 * and packet handling for ESP-SDR streaming under ESP-IDF.
 */

#ifndef ESP_SDR_VRT_PACKET_H_
#define ESP_SDR_VRT_PACKET_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "iq_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

/* VITA 49 / VITA 49.2 Packet Types */
typedef enum {
    VRT_PT_IF_DATA_WITHOUT_STREAM_ID = 0x0,
    VRT_PT_IF_DATA_WITH_STREAM_ID    = 0x1,
    VRT_PT_EXT_DATA_WITHOUT_STREAM_ID = 0x2,
    VRT_PT_EXT_DATA_WITH_STREAM_ID   = 0x3,
    VRT_PT_IF_CONTEXT                = 0x4,
    VRT_PT_EXT_CONTEXT               = 0x5,
} vrt_packet_type_t;

/* Timestamp Integer (TSI) */
typedef enum {
    VRT_TSI_NONE  = 0x0,
    VRT_TSI_UTC   = 0x1,
    VRT_TSI_GPS   = 0x2,
    VRT_TSI_OTHER = 0x3,
} vrt_tsi_t;

/* Timestamp Fractional (TSF) */
typedef enum {
    VRT_TSF_NONE               = 0x0,
    VRT_TSF_SAMPLE_COUNT       = 0x1,
    VRT_TSF_REAL_TIME          = 0x2, /* picoseconds */
    VRT_TSF_FREE_RUNNING_COUNT = 0x3,
} vrt_tsf_t;

/* VITA 49.2 Spectrum or Time (S indicator) */
typedef enum {
    VRT_SOT_TIME     = 0x0,
    VRT_SOT_SPECTRUM = 0x1,
} vrt_spectrum_or_time_t;

/* Timestamp Mode (TSM) */
typedef enum {
    VRT_TSM_FINE   = 0x0,
    VRT_TSM_COARSE = 0x1,
} vrt_tsm_t;

/* VITA 49 Compliance */
typedef enum {
    VRT_V49_0 = 0x0,
    VRT_V49_2 = 0x2,
} vrt_compliance_t;

/* Payload Packing Method */
typedef enum {
    VRT_PM_PROCESSING_EFFICIENT = 0x0,
    VRT_PM_LINK_EFFICIENT       = 0x1,
} vrt_packing_method_t;

/* Real / Complex Format */
typedef enum {
    VRT_ROC_REAL              = 0x0,
    VRT_ROC_COMPLEX_CARTESIAN = 0x1,
    VRT_ROC_COMPLEX_POLAR     = 0x2,
} vrt_real_or_complex_t;

/* Data Item Format */
typedef enum {
    VRT_DIF_SIGNED_FIXED_POINT   = 0x0,
    VRT_DIF_SIGNED_VFLOAT        = 0x1,
    VRT_DIF_IEEE754              = 0x2,
    VRT_DIF_UNSIGNED_VFLOAT      = 0x3,
    VRT_DIF_UNSIGNED_FIXED_POINT = 0x4,
} vrt_data_item_format_t;

/* VITA 49.2 Header representation */
typedef struct {
    vrt_packet_type_t packet_type;
    bool has_class_id;
    bool has_trailer;
    bool nd0;                        /* VITA 49.2: 1 = not V49.0 (uses 49.2 features) */
    vrt_spectrum_or_time_t spectrum_or_time; /* 0 = time domain, 1 = spectrum */
    vrt_tsm_t tsm;
    vrt_tsi_t tsi;
    vrt_tsf_t tsf;
    uint8_t packet_count;            /* 0..15 */
    uint16_t packet_size;            /* in 32-bit words */
} vrt_header_t;

/* VITA 49.2 Trailer representation */
typedef struct {
    bool has_calibrated_time;
    bool calibrated_time;
    bool has_valid_data;
    bool valid_data;
    bool has_reference_lock;
    bool reference_lock;
    bool has_agc_or_mgc;
    bool agc_or_mgc;                 /* true: AGC, false: MGC */
    bool has_detected_signal;
    bool detected_signal;
    bool has_spectral_inversion;
    bool spectral_inversion;
    bool has_over_range;
    bool over_range;                 /* clipped */
    bool has_sample_loss;
    bool sample_loss;                /* gap/discontinuity */
    bool has_context_packet_count;
    uint8_t context_packet_count;
} vrt_trailer_t;

/* Data Overhead in 32-bit words: Header(1) + Stream ID(1) + TSI(1) + TSF(2) + Trailer(1) */
#define VRT_DATA_OVERHEAD_WORDS 6U

/**
 * Encode a VITA 49 / 49.2 header into a 32-bit word (host CPU byte order).
 *
 * @param hdr Pointer to header structure.
 * @return 32-bit header word.
 */
uint32_t vrt_pack_header(const vrt_header_t *hdr);

/**
 * Decode a 32-bit word (host CPU byte order) into a VITA 49 / 49.2 header.
 *
 * @param word 32-bit header word.
 * @param hdr Pointer to destination header structure.
 * @return true if valid, false if pointers NULL.
 */
bool vrt_unpack_header(uint32_t word, vrt_header_t *hdr);

/**
 * Encode a VITA 49 / 49.2 trailer into a 32-bit word (host CPU byte order).
 *
 * @param tr Pointer to trailer structure.
 * @return 32-bit trailer word.
 */
uint32_t vrt_pack_trailer(const vrt_trailer_t *tr);

/**
 * Decode a 32-bit word (host CPU byte order) into a VITA 49 / 49.2 trailer.
 *
 * @param word 32-bit trailer word.
 * @param tr Pointer to destination trailer structure.
 * @return true if valid, false if pointers NULL.
 */
bool vrt_unpack_trailer(uint32_t word, vrt_trailer_t *tr);

/**
 * Split a nanosecond timestamp into integer seconds and picoseconds fraction.
 *
 * @param ns Timestamp in nanoseconds.
 * @param integer_sec Output pointer for integer seconds.
 * @param fractional_ps Output pointer for fractional seconds in picoseconds.
 */
void vrt_split_time(uint64_t ns, uint32_t *integer_sec, uint64_t *fractional_ps);

/**
 * Calculate sample timestamp with offset and sample rate with picosecond precision.
 *
 * @param base_ns Base timestamp in nanoseconds.
 * @param sample_offset Sample index offset within burst.
 * @param rate_hz Sample rate in Hz.
 * @param integer_sec Output pointer for integer seconds.
 * @param fractional_ps Output pointer for fractional seconds in picoseconds.
 */
void vrt_split_burst_time(uint64_t base_ns, uint32_t sample_offset, uint32_t rate_hz,
                          uint32_t *integer_sec, uint64_t *fractional_ps);

/**
 * Build standard VITA 49.2 IF Data packet header fields into a 32-bit word buffer.
 *
 * Layout (5 words):
 *   buf[0] = Header word (Type=0x1, TSI=OTHER, TSF=REAL_TIME, ND0=1, Trailer=has_trailer, packet_count, packet_size)
 *   buf[1] = Stream ID
 *   buf[2] = Integer seconds timestamp
 *   buf[3] = Fractional seconds (upper 32 bits of picoseconds)
 *   buf[4] = Fractional seconds (lower 32 bits of picoseconds)
 *
 * @param buf Output buffer for header words (must hold at least 5 words).
 * @param max_words Capacity of buf.
 * @param stream_id 32-bit VITA Stream ID.
 * @param packet_count Modulo-16 packet counter (0..15).
 * @param integer_sec Integer seconds.
 * @param fractional_ps Fractional seconds in picoseconds.
 * @param payload_words Number of payload words.
 * @param has_trailer Whether trailer word will be included.
 * @return Number of header words written (5), or 0 on error.
 */
size_t vrt_build_if_data_header(uint32_t *buf, size_t max_words, uint32_t stream_id,
                                uint8_t packet_count, uint32_t integer_sec,
                                uint64_t fractional_ps, size_t payload_words,
                                bool has_trailer);

/**
 * Build standard VITA 49.2 IF Data trailer word.
 *
 * @param valid_data True if sample data is valid.
 * @param agc True if AGC was active, false if manual gain (MGC).
 * @param clipped True if any sample in packet clipped / reached full scale.
 * @param sample_loss True if sample discontinuity / packet gap occurred.
 * @return 32-bit trailer word.
 */
uint32_t vrt_build_if_data_trailer(bool valid_data, bool agc, bool clipped,
                                   bool sample_loss);

/**
 * Build a complete VITA 49.2 IF Data packet from complex samples.
 *
 * @param out_packet Destination buffer for the complete packet (words).
 * @param max_words Buffer capacity in 32-bit words.
 * @param stream_id 32-bit Stream ID.
 * @param packet_count Modulo-16 packet counter.
 * @param timestamp_ns Base timestamp in nanoseconds.
 * @param sample_offset Offset of the first sample in this packet.
 * @param rate_hz Complex sample rate in Hz.
 * @param samples Input complex samples.
 * @param num_samples Number of samples to pack.
 * @param bits Quantization bits (8, 12, or 16).
 * @param full_scale Native full scale (> 0).
 * @param agc True if AGC active.
 * @param sample_loss True if sample loss / discontinuity flagged.
 * @param network_byte_order If true, swaps all words to big-endian (network order).
 * @return Total packet words written, or 0 on error.
 */
size_t vrt_build_data_packet(uint32_t *out_packet, size_t max_words, uint32_t stream_id,
                             uint8_t packet_count, uint64_t timestamp_ns,
                             uint32_t sample_offset, uint32_t rate_hz,
                             const struct esp_sdr_iq16 *samples, size_t num_samples,
                             unsigned int bits, int32_t full_scale, bool agc,
                             bool sample_loss, bool network_byte_order);

/**
 * Build a standard VITA 49.2 IF Context Packet (CIF0 + CIF1) with compliance,
 * sample rate, bandwidth, RF frequency, and payload format.
 *
 * @param out_packet Buffer for context packet words.
 * @param max_words Buffer capacity in words (must be at least 15 words).
 * @param stream_id 32-bit Stream ID.
 * @param packet_count Modulo-16 packet counter.
 * @param timestamp_ns Timestamp in nanoseconds.
 * @param freq_hz RF center frequency in Hz.
 * @param rate_hz Sample rate in Hz.
 * @param bandwidth_hz Bandwidth in Hz.
 * @param bits Bits per item (8, 12, 16).
 * @param changed True if context parameters changed since last context packet.
 * @param network_byte_order If true, swaps words to big-endian.
 * @return Total packet words written, or 0 on error.
 */
size_t vrt_build_context_packet(uint32_t *out_packet, size_t max_words,
                                uint32_t stream_id, uint8_t packet_count,
                                uint64_t timestamp_ns, uint64_t freq_hz,
                                uint32_t rate_hz, uint32_t bandwidth_hz,
                                unsigned int bits, bool changed,
                                bool network_byte_order);

/**
 * Swap a buffer of 32-bit words between host byte order and big-endian (network byte order).
 *
 * @param words Buffer of 32-bit words.
 * @param count Number of words.
 */
void vrt_to_big_endian(uint32_t *words, size_t count);
void vrt_from_big_endian(uint32_t *words, size_t count);

#ifndef STREAM_PAYLOAD
#define STREAM_PAYLOAD 1344
#endif

/*
 * Layout of ESP-SDR native wire streaming packet (compatible with stream.h stream_packet_t)
 */
typedef struct __attribute__((packed)) {
    char magic[4];
    uint16_t version, bits;
    uint32_t epoch, rate;
    uint64_t sample, time_us;
    int8_t iq[STREAM_PAYLOAD];
} esp_sdr_stream_pkt_t;

/**
 * Convert an ESP-SDR native stream packet (from stream.h) to a VITA 49.2 IF Data packet.
 *
 * @param stream_pkt Pointer to esp_sdr_stream_pkt_t or stream_packet_t.
 * @param vrt_buf Buffer to store the output VITA 49.2 packet.
 * @param max_words Buffer capacity in 32-bit words.
 * @param stream_id VITA Stream ID to assign.
 * @param packet_count Modulo-16 packet counter.
 * @param gain Gain index (< 0 for AGC).
 * @param sample_loss True if sample loss is flagged.
 * @param network_byte_order If true, swaps output to big-endian.
 * @return Number of words written, or 0 on error.
 */
size_t vrt_stream_packet_to_v49_data(const void *stream_pkt, uint32_t *vrt_buf,
                                     size_t max_words, uint32_t stream_id,
                                     uint8_t packet_count, int gain,
                                     bool sample_loss, bool network_byte_order);

#ifdef __cplusplus
}
#endif

#endif /* ESP_SDR_VRT_PACKET_H_ */
