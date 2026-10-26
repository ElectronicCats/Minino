/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SDR build linker cap: shrink the SRAM flow segment so no application
 * IRAM/DRAM section can land inside the C6 capture bank. The dump engine
 * takes the whole of SRAM block 2 (0x40840000..0x40860000) during a
 * capture, so nothing may live there - upstream reserves that block in
 * its linker script (esp-sdr-zephyr lib/esp_sdr/esp_sdr_bank_c6.ld) and
 * here we do the same by redefining the length of IDF's sram_seg.
 *
 * The region is redefined (and not "LENGTH(sram_seg) = ...": that form does
 * not exist in this binutils) with the same origin and a shorter length, so
 * every section placed with "> sram_seg" has to fit below the bank. A DRAM
 * image that does not fit fails the link with "region 'sram_seg' overflowed
 * by N bytes", which is the forcing function for the SDR build variant: it
 * must free enough DRAM to fit below the bank. SRAM block 3 above the bank
 * is left to the heap: heap port memory_layout.c lists it as a CPU RAM
 * block, so heap_caps picks it up automatically once the SDR bank is
 * reserved from it (esp_sdr_memory.c).
 *
 * Appended with -T after IDF's own linker scripts, so this definition wins
 * over the one in memory.ld.
 */

MEMORY
{
  sram_seg (RWX) : org = 0x40800000, len = 0x40000
}

ASSERT(_heap_start <= 0x40840000, "SDR: the DRAM image overlaps the capture bank")
