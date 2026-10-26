/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SDR build memory layout. The C6 dump engine owns the whole of SRAM
 * block 2 (0x40840000..0x40860000) while a capture runs and writes the
 * IQ samples at the start of that block: any CPU data living there is
 * either corrupted (engine writes) or deadlocks the bus (CPU access
 * while the block is owned). ESP-IDF lists that block as regular heap
 * RAM, so it must be taken off the heap: SOC_RESERVE_MEMORY_REGION()
 * emits a soc_reserved_region_t into the .reserved_memory_address
 * section, the same section IDF's heap port (components/heap/port/
 * esp32c6/memory_layout.c) uses to remove reserved ranges when it
 * registers the boot heaps.
 *
 * The capture buffer itself stays at the fixed address 0x40840000
 * (bank front, exactly what the engine writes), defined in
 * c6_sdr_rx.c. The engine stops at the programmed word count, so only
 * the first 64 KiB of the block is taken off the heap: the BLE
 * controller needs one large contiguous block and the rest of the bank
 * is what the heap is missing. Nothing else needs linker script support: the
 * section reserve flows through the same mechanism as every other reserved
 * range.
 *
 * Only compiled in the SDR build variant (CONFIG_SDR_ENABLE), where
 * the linker cap (esp_sdr_bank.ld) forces the DRAM image below the
 * bank and frees BLE to make room.
 *
 * The functions below are not there for their own sake: the reservation
 * lives in a static variable of this translation unit, and a static in its
 * own section is dropped together with the object file if nothing
 * references it. The RX engine asks for the bank base, so this file is
 * linked and the reservation reaches the linker.
 */

#include <stdint.h>
#include "sdkconfig.h"

#if defined(CONFIG_SDR_ENABLE)
  #include "heap_memory_layout.h"
#endif
#include "esp_sdr_memory.h"

#if defined(CONFIG_SDR_ENABLE)
/* Capture bank: the front of SRAM block 2, the part the engine writes. */
SOC_RESERVE_MEMORY_REGION((intptr_t) SDR_CAPTURE_BANK_BASE,
                          (intptr_t) (SDR_CAPTURE_BANK_BASE +
                                      SDR_CAPTURE_BANK_SIZE),
                          sdr_capture_bank);
#endif

uint32_t sdr_capture_bank_base(void) {
  return SDR_CAPTURE_BANK_BASE;
}

uint32_t sdr_capture_bank_size(void) {
  return SDR_CAPTURE_BANK_SIZE;
}
