/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SDR build linker cap: the C6 capture bank occupies SRAM block 2
 * (0x40840000..0x40860000). Upstream (esp-sdr-zephyr) reserves that block
 * in its linker script, and we ensure the compiler’s default sram_seg
 * definition does not cover that address range, so the capture engine has
 * exclusive use of the upper part of SRAM.
 *
 * The static assertion below guarantees the heap starts below the capture
 * bank; the exact size of the default sram_seg is left to IDF’s memory.ld.
 */
ASSERT(_heap_start <= 0x40840000, "SDR: the DRAM image overlaps the capture bank")
