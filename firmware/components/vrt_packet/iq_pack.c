/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Complex I/Q sample packing/unpacking: 8, 12, 16 bits.
 */

#include "iq_pack.h"

#define CLAMP(x, low, high) (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))

size_t iq_samples_per_words(unsigned int bits, size_t max_words) {
    if (!iq_bits_valid(bits)) {
        return 0;
    }
    size_t n = max_words * 32U / (2U * bits);
    return n - (n % iq_sample_quantum(bits));
}

static inline uint32_t item(int32_t x, int32_t shift, int32_t lim, uint32_t mask) {
    if (shift > 0) {
        x = (x + (1 << (shift - 1))) >> shift;
    } else {
        x <<= -shift;
    }
    return (uint32_t)CLAMP(x, -lim, lim - 1) & mask;
}

size_t iq_pack(const struct esp_sdr_iq16 *s, size_t n, unsigned int bits, int32_t full_scale,
               uint32_t *words) {
    if (!iq_bits_valid(bits) || s == NULL || words == NULL || n == 0U || full_scale <= 0) {
        return 0;
    }
    int32_t lim = 1 << (bits - 1U);
    uint32_t mask = (1U << bits) - 1U;
    int32_t shift = (int32_t)(31 - __builtin_clz((uint32_t)full_scale)) - (int32_t)(bits - 1U);
    uint64_t acc = 0;
    unsigned int have = 0;
    size_t w = 0, j = 0;

    if (bits == 16U) {
        for (; j < n; j++) {
            words[j] = ((uint32_t)(uint16_t)s[j].i << 16) | (uint16_t)s[j].q;
        }
        return n;
    }

    if (bits == 8U) {
        for (; j + 2U <= n; j += 2U) {
            words[w++] = item(s[j].i, shift, lim, mask) << 24 |
                         item(s[j].q, shift, lim, mask) << 16 |
                         item(s[j + 1U].i, shift, lim, mask) << 8 |
                         item(s[j + 1U].q, shift, lim, mask);
        }
    } else if (bits == 12U) {
        for (; j + 4U <= n; j += 4U) {
            uint32_t a = item(s[j].i, shift, lim, mask);
            uint32_t b = item(s[j].q, shift, lim, mask);
            uint32_t c = item(s[j + 1U].i, shift, lim, mask);
            uint32_t d = item(s[j + 1U].q, shift, lim, mask);
            uint32_t e = item(s[j + 2U].i, shift, lim, mask);
            uint32_t f = item(s[j + 2U].q, shift, lim, mask);
            uint32_t g = item(s[j + 3U].i, shift, lim, mask);
            uint32_t h = item(s[j + 3U].q, shift, lim, mask);

            words[w++] = (a << 20) | (b << 8) | (c >> 4);
            words[w++] = (c << 28) | (d << 16) | (e << 4) | (f >> 8);
            words[w++] = (f << 24) | (g << 12) | h;
        }
    }

    /* Tail handling */
    for (; j < n; j++) {
        acc = (acc << (2U * bits)) | ((uint64_t)item(s[j].i, shift, lim, mask) << bits) |
              item(s[j].q, shift, lim, mask);
        have += 2U * bits;
        while (have >= 32U) {
            have -= 32U;
            words[w++] = (uint32_t)(acc >> have);
        }
    }
    if (have > 0U) {
        words[w++] = (uint32_t)(acc << (32U - have));
    }
    return w;
}

size_t iq_unpack(const uint32_t *words, size_t nwords, unsigned int bits, int16_t *iq,
                 size_t max) {
    if (!iq_bits_valid(bits) || words == NULL || iq == NULL || max == 0U || nwords == 0U) {
        return 0;
    }
    uint64_t acc = 0;
    unsigned int have = 0;
    size_t n = 0;

    if (bits == 16U) {
        for (; n < nwords && n < max; n++) {
            iq[2U * n] = (int16_t)(words[n] >> 16);
            iq[2U * n + 1U] = (int16_t)(words[n] & 0xffffU);
        }
        return n;
    }

    for (size_t k = 0; k < nwords && n < max; k++) {
        acc = (acc << 32) | words[k];
        have += 32U;
        while (have >= 2U * bits && n < max) {
            uint32_t pair = (uint32_t)(acc >> (have - 2U * bits));
            have -= 2U * bits;
            iq[2U * n] = (int16_t)((int32_t)((pair >> bits) << (32U - bits)) >> 16);
            iq[2U * n + 1U] = (int16_t)((int32_t)(pair << (32U - bits)) >> 16);
            n++;
        }
    }
    return n;
}

size_t iq_pack_raw(const struct esp_sdr_iq16 *s, size_t n, unsigned int bits, uint32_t *words) {
    if (!iq_bits_valid(bits) || s == NULL || words == NULL || n == 0U) {
        return 0;
    }
    int32_t full_scale = (int32_t)(1U << (bits - 1U));
    return iq_pack(s, n, bits, full_scale, words);
}

size_t iq_unpack_raw(const uint32_t *words, size_t nwords, unsigned int bits, struct esp_sdr_iq16 *s,
                     size_t max) {
    if (!iq_bits_valid(bits) || words == NULL || s == NULL || max == 0U || nwords == 0U) {
        return 0;
    }
    uint64_t acc = 0;
    unsigned int have = 0;
    size_t n = 0;
    uint32_t mask = (1U << bits) - 1U;

    if (bits == 16U) {
        for (; n < nwords && n < max; n++) {
            s[n].i = (int16_t)(words[n] >> 16);
            s[n].q = (int16_t)(words[n] & 0xffffU);
        }
        return n;
    }

    for (size_t k = 0; k < nwords && n < max; k++) {
        acc = (acc << 32) | words[k];
        have += 32U;
        while (have >= 2U * bits && n < max) {
            uint32_t pair = (uint32_t)(acc >> (have - 2U * bits));
            have -= 2U * bits;
            uint32_t raw_i = (pair >> bits) & mask;
            uint32_t raw_q = pair & mask;

            if (bits == 8U) {
                s[n].i = (int16_t)(int8_t)raw_i;
                s[n].q = (int16_t)(int8_t)raw_q;
            } else if (bits == 12U) {
                s[n].i = (int16_t)((int16_t)((uint16_t)raw_i << 4) >> 4);
                s[n].q = (int16_t)((int16_t)((uint16_t)raw_q << 4) >> 4);
            }
            n++;
        }
    }
    return n;
}
