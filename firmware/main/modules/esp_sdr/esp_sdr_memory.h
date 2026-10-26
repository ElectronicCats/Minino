/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The C6 capture bank: SRAM block 2, which the dump engine owns while a
 * capture runs.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Start of the capture bank (block 2, 128 KiB). */
#define SDR_CAPTURE_BANK_BASE 0x40840000u
/*
 * Size of the capture bank.
 *
 * The whole block has to stay out of the heap, not only the part the
 * engine writes. The dump engine takes the ownership of the entire 128 KiB
 * block while a capture runs (the usage bit in SRAM_OWNER_REG is per
 * block, not per page), and while the engine owns it the CPUs cannot reach
 * any of it: whatever the heap had put in the upper half of the block
 * becomes inaccessible, the Wi-Fi driver hangs waiting for its buffers and
 * the task watchdog fires. Reserving 64 KiB and letting the heap use the
 * rest of the block is exactly that bug.
 */
#define SDR_CAPTURE_BANK_SIZE 0x20000u

/**
 * @brief Base address the dump engine writes the I/Q samples to
 *
 * The engine writes at the front of the block it owns, so this is the
 * address of the first sample.
 */
uint32_t sdr_capture_bank_base(void);

/** @brief Size of the capture bank in bytes. */
uint32_t sdr_capture_bank_size(void);

#ifdef __cplusplus
}
#endif
