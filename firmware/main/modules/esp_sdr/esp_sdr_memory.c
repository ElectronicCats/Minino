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
 * c6_sdr_rx.c. The whole block is reserved, not only the part the
 * engine writes (see SDR_CAPTURE_BANK_SIZE for why).
 *
 * One image carries SDR and every other app, so the bank is reserved
 * from the heap at boot and handed back to it straight away, unless this
 * is an SDR boot: the SDR app reboots into one (sdr_bank_reboot_into_sdr)
 * and its exit reboots back. The other apps get the whole heap; the SDR
 * gets a bank nothing has touched. The request lives in RTC no-init
 * memory and is consumed on boot, so a crash or a power cycle in SDR mode
 * comes back as a normal boot.
 *
 * With CONFIG_SDR_ENABLE off nothing is reserved and the bank is never
 * owned by the engine (c6_sdr_rx_init() refuses to start).
 */

#include <stdbool.h>
#include <stdint.h>
#include "sdkconfig.h"

#include "esp_attr.h"
#include "esp_heap_caps_init.h"
#include "esp_log.h"
#include "esp_system.h"
#if defined(CONFIG_SDR_ENABLE)
  #include "heap_memory_layout.h"
  #include "soc/usb_serial_jtag_struct.h"
#endif
#include "esp_sdr_memory.h"

static const char* TAG = "sdr_bank";

#if defined(CONFIG_SDR_ENABLE)
/* Capture bank: the whole of SRAM block 2 (see esp_sdr_memory.h). */
SOC_RESERVE_MEMORY_REGION((intptr_t) SDR_CAPTURE_BANK_BASE,
                          (intptr_t) (SDR_CAPTURE_BANK_BASE +
                                      SDR_CAPTURE_BANK_SIZE),
                          sdr_capture_bank);

  #define SDR_BOOT_MAGIC 0x5D2B0071u
static RTC_NOINIT_ATTR uint32_t s_sdr_boot_request;
#endif

static bool s_bank_reserved = false;

bool sdr_bank_boot(void) {
#if defined(CONFIG_SDR_ENABLE)
  bool sdr_boot = s_sdr_boot_request == SDR_BOOT_MAGIC &&
                  esp_reset_reason() == ESP_RST_SW;
  s_sdr_boot_request = 0;
  if (sdr_boot) {
    s_bank_reserved = true;
    /* The ESPARGOS viewer resets the chip over USB on its second SYNC. That
       reset lands here, before the SDR app turns it off, and would come
       back as a normal boot: turn it off now (esp_sdr_module_stop turns it
       back on). */
    USB_SERIAL_JTAG.chip_rst.usb_uart_chip_rst_dis = 1;
    USB_SERIAL_JTAG.config_update.config_update = 1;
    ESP_LOGI(TAG, "SDR boot: the capture bank stays out of the heap");
    return true;
  }
  esp_err_t err = heap_caps_add_region(
      (intptr_t) SDR_CAPTURE_BANK_BASE,
      (intptr_t) (SDR_CAPTURE_BANK_BASE + SDR_CAPTURE_BANK_SIZE));
  /* If the heap did not take it, the bank is still free for the engine. */
  s_bank_reserved = (err != ESP_OK);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "the capture bank was not given to the heap: %s",
             esp_err_to_name(err));
  }
#endif
  return false;
}

bool sdr_bank_is_reserved(void) {
  return s_bank_reserved;
}

void sdr_bank_reboot_into_sdr(void) {
#if defined(CONFIG_SDR_ENABLE)
  s_sdr_boot_request = SDR_BOOT_MAGIC;
  esp_restart();
#else
  ESP_LOGE(TAG, "SDR is not in this build (CONFIG_SDR_ENABLE)");
#endif
}

uint32_t sdr_capture_bank_base(void) {
  return SDR_CAPTURE_BANK_BASE;
}

uint32_t sdr_capture_bank_size(void) {
  return SDR_CAPTURE_BANK_SIZE;
}
