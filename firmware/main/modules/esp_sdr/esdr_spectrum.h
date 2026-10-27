/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * On-chip computed spectra: the "On-Chip Spectrum" mode of the ESPARGOS
 * viewer.
 *
 * Instead of sending raw samples for the browser to compute the FFT, the
 * board computes the spectrum and sends a list of power codes. This allows
 * the viewer to refresh at dozens of frames per second instead of ten.
 *
 * Ported from ESPARGOS firmware (GPL-3.0-or-later): main/common/spectrum.c,
 * spectrum_dc.h, spectrum_math.h and spectrum_fft.h, using the format
 * described in its docs/spectrum.md.
 */

#ifndef ESDR_SPECTRUM_H_
#define ESDR_SPECTRUM_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of FFT bins supported by the firmware. */
#define ESDR_SPEC_MAX_FFT 2048u

/**
 * @brief Acquire a burst and return the raw words.
 *
 * @param n          number of samples
 * @param rate       rate code (0 = 80 MS/s, 1 = 40 MS/s, 6 = 16 MS/s)
 * @param words      raw words (I in bits 9-0, Q in bits 19-10)
 * @param elapsed_us microseconds taken by the capture
 * @return true if the capture succeeded
 */
typedef bool (*esdr_spectrum_acquire_fn)(uint32_t n,
                                         uint32_t rate,
                                         const uint32_t** words,
                                         uint32_t* elapsed_us);

/**
 * @brief Handle spectrum commands.
 *
 * @param line        received command line
 * @param freq_mhz    frequency requested by the client
 * @param acquire     callback to acquire a burst
 * @return true if the line was a spectrum command (replied or not)
 */
bool esdr_spectrum_command(const char* line,
                           uint32_t freq_mhz,
                           esdr_spectrum_acquire_fn acquire);

#ifdef __cplusplus
}
#endif

#endif /* ESDR_SPECTRUM_H_ */
