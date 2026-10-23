/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ESP32-C6 Software Defined Radio - Receive (RX) Driver
 * In-process Dump Engine burst capture for Minino OS.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_sdr_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C6_SDR_MAX_SAMPLES 16380u

/* The dump engine takes a clock divider in bits 15-16 of the control
   register. The rate codes are the ones the ESP-SDR burst protocol uses:
   0 is 80 MS/s, 1 is 40 MS/s and 6 is 16 MS/s. */
#define C6_SDR_RATE_80MSPS 0u
#define C6_SDR_RATE_40MSPS 1u
#define C6_SDR_RATE_16MSPS 6u

/* The engine writes the words at the front of the capture bank: a capture
 * that does not fit in the reserved bytes would run into the heap. */
_Static_assert(C6_SDR_MAX_SAMPLES * sizeof(uint32_t) <= SDR_CAPTURE_BANK_SIZE,
               "the capture does not fit in the reserved bank");

/**
 * @brief Initialize SDR RX hardware and internal registers on ESP32-C6.
 */
esp_err_t c6_sdr_rx_init(void);

/**
 * @brief Tune receiver to center frequency in MHz (e.g. 2412 for Wi-Fi CH1).
 */
esp_err_t c6_sdr_rx_tune(uint32_t freq_mhz);

/**
 * @brief Get current receiver center frequency in MHz.
 */
uint32_t c6_sdr_rx_get_freq(void);

/**
 * @brief Capture a burst of raw I/Q samples into internal buffer.
 *
 * @param num_samples Number of words to capture (256 to 16380).
 * @param capture_us Optional pointer to store elapsed capture time in
 * microseconds.
 * @return true on success, false on timeout or error.
 */
bool c6_sdr_rx_capture(uint32_t num_samples, uint32_t* capture_us);

/**
 * @brief Capture a burst at one of the rates the dump engine supports.
 *
 * @param num_samples Number of words to capture (256 to 16380).
 * @param rate One of C6_SDR_RATE_80MSPS, C6_SDR_RATE_40MSPS or
 *             C6_SDR_RATE_16MSPS.
 * @param capture_us Optional pointer to store elapsed capture time in
 * microseconds.
 * @return true on success, false on timeout or error.
 */
bool c6_sdr_rx_capture_rate(uint32_t num_samples,
                            uint32_t rate,
                            uint32_t* capture_us);

/**
 * @brief Get pointer to the internal I/Q sample buffer.
 */
const uint32_t* c6_sdr_rx_get_buffer(void);

/**
 * @brief Stop SDR RX hardware and restore SRAM / PHY configuration.
 */
void c6_sdr_rx_stop(void);

/**
 * @brief Check if SDR RX hardware is active.
 */
bool c6_sdr_rx_is_active(void);

/**
 * @brief Start continuous wave (CW) RF carrier transmission on current
 * frequency.
 */
esp_err_t c6_sdr_tx_carrier_start(void);

/**
 * @brief Stop continuous wave (CW) RF carrier transmission and restore RX
 * chain.
 */
void c6_sdr_tx_carrier_stop(void);

/**
 * @brief Check if RF carrier transmission is active.
 */
bool c6_sdr_tx_carrier_is_active(void);

#ifdef __cplusplus
}
#endif
