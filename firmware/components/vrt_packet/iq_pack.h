/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Complex I/Q sample packing/unpacking: 8, 12, 16 bits.
 */

#ifndef IQ_PACK_H_
#define IQ_PACK_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Complex sample, I then Q. */
struct esp_sdr_iq16 {
    int16_t i;
    int16_t q;
};

/** Supported item bit depths (8, 12, 16). */
static inline int iq_bits_valid(unsigned int bits) {
    return (bits == 8U || bits == 12U || bits == 16U);
}

/** Samples per packet quantum to prevent phantom fractional samples. */
static inline size_t iq_sample_quantum(unsigned int bits) {
    return (bits == 12U ? 4U : (bits == 8U ? 2U : 1U));
}

/** Most samples (a whole quantum) that fit into max_words 32-bit words. */
size_t iq_samples_per_words(unsigned int bits, size_t max_words);

/**
 * Pack n samples into 32-bit words.
 * 16-bit items keep scale; 8 and 12-bit items are scaled to their own
 * full scale (2^(bits-1)), rounded and saturated.
 * Returns words written.
 */
size_t iq_pack(const struct esp_sdr_iq16 *s, size_t n, unsigned int bits, int32_t full_scale,
               uint32_t *words);

/**
 * Unpack nwords into interleaved int16 I, Q (8 and 12-bit items shifted up to
 * full scale 32767, 16-bit items as they are).
 * Returns samples written, at most max.
 */
size_t iq_unpack(const uint32_t *words, size_t nwords, unsigned int bits, int16_t *iq,
                 size_t max);

/**
 * Lossless pack/unpack of raw native samples (8-bit: -128..127, 12-bit: -2048..2047, 16-bit: -32768..32767).
 */
size_t iq_pack_raw(const struct esp_sdr_iq16 *s, size_t n, unsigned int bits, uint32_t *words);
size_t iq_unpack_raw(const uint32_t *words, size_t nwords, unsigned int bits, struct esp_sdr_iq16 *s,
                     size_t max);

#ifdef __cplusplus
}
#endif

#endif /* IQ_PACK_H_ */
