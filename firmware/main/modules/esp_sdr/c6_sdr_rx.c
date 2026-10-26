/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ESP32-C6 Software Defined Radio - Receive (RX) Driver
 * In-process Dump Engine burst capture for Minino OS.
 */

#include "c6_sdr_rx.h"
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_sdr_memory.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "radio_lease.h"
#include "soc/soc.h"
#include "wifi_controller.h"

static const char* TAG = "c6_sdr_rx";

/* Hardware registers */
#define SRAM_OWNER_REG 0x60095004u
#define MODEM_CLK_REG1 0x600a9804u
#define MODEM_CLK_REG2 0x600a9814u
#define MODEM_CLK_REG3 0x600a980cu
#define DUMP_CTRL_REG  0x600a9004u
#define DUMP_CFG_REG1  0x600a9014u
#define DUMP_CFG_REG2  0x600a9008u
#define DUMP_FE_REG    0x600a20b4u

/* PHY / ROM external functions */
extern void pbus_workmode(void);
extern void rom_pbus_xpd_tx_off(void);
extern void rom_pbus_xpd_rx_off(void);
extern void ram_pbus_xpd_rx_on(unsigned);
extern void ram_pbus_xpd_tx_on(unsigned);
extern void set_rxclk_en(unsigned);
extern void phy_force_rx_gain(unsigned, unsigned);
/* El tono del cert-test y el programador de canal del PHY. Los dos hacen
   falta para que la entrada del conversor sea una señal y no un valor
   fijo: ver c6_sdr_rx_tune(). */
extern void ram_stop_tx_tone(unsigned stop);

/* The dump engine writes at the front of the block it owns: the capture bank
   (see esp_sdr_memory.c, which also keeps that block off the heap). */
static volatile uint32_t* s_iq_buf = NULL;
static bool s_rx_active = false;
static bool s_we_started_wifi = false;
static bool s_carrier_tx_active = false;
static uint32_t s_current_freq = 2412;
static uint32_t s_saved_sram_owner = 0;
static bool s_gain_manual = false;
static uint32_t s_gain_index = 40;

esp_err_t c6_sdr_rx_init(void) {
  if (s_rx_active) {
    return ESP_OK;
  }

  s_iq_buf = (volatile uint32_t*) sdr_capture_bank_base();
  ESP_LOGI(TAG, "Initializing C6 native SDR RX engine (buffer at 0x%08X)...",
           sdr_capture_bank_base());

  /* The dump engine borrows the Wi-Fi PHY, so the radio has to be up. If
     BLE holds it there is no room left for the driver buffers and the
     driver says so: report it instead of dying with it. */
  if (!wifi_driver_get_initialized()) {
    if (!wifi_driver_init_sta()) {
      ESP_LOGE(TAG,
               "the Wi-Fi driver did not come up: stop the app that "
               "holds the radio (BLE) and try again");
      return ESP_ERR_NO_MEM;
    }
    s_we_started_wifi = true;
  } else {
    s_we_started_wifi = false;
  }

  /* Keeps the PHY clocks and the calibration alive with a plain Wi-Fi claim.
     The promiscuous mode is asked for below, through the lease: the dump
     engine needs the receiver chain of the PHY to stay up. */
  esp_err_t err = radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "wifi claim: %s", esp_err_to_name(err));
    return err;
  }

  /* El radio tiene que quedarse encendido y sin gestión de energía. El
     firmware del ESPARGOS arranca el Wi-Fi sin power save y en promiscuo
     (receiver.c:292-298): con el driver en STA el radio gestiona su propia
     vida (se duerme, escanea, se asocia) y la captura sale con un valor
     constante en vez de señal.

     El modo promiscuo se pide con la lease, como el resto de los que
     consumen el radio: así el driver sólo se toca al pasar de 0 a 1
     tenedores y las otras apps que lo comparten no se lo quitan de
     debajo. */
  esp_wifi_set_ps(WIFI_PS_NONE);
  err = radio_lease_prom_hold(RADIO_PROM_SDR_RX);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "promiscuous claim: %s", esp_err_to_name(err));
    radio_lease_wifi_release();
    return err;
  }

  s_saved_sram_owner = REG_READ(SRAM_OWNER_REG);

  /* Configure initial tune */
  c6_sdr_rx_tune(s_current_freq);
  s_rx_active = true;

  return ESP_OK;
}

/* La cadena de RX del PHY, tal y como la monta el firmware del ESPARGOS:
   el tono del cert-test apagado, el canal puesto en el PHY y la cadena de
   RX encendida. El firmware del ESPARGOS la rehace antes de cada captura
   (prepare_rx()), y hace falta: el driver de Wi-Fi puede mover el canal
   por su cuenta y la captura salía de un canal que no era el pedido. */
static void rx_front_end(uint32_t freq_mhz) {
  /* El tono del cert-test tiene que estar apagado: con el tono encendido
     la entrada del conversor es una portadora constante y la captura sale
     con un valor fijo en vez de señal. */
  ram_stop_tx_tone(1);

  /* El canal lo deja el driver de Wi-Fi, y tiene que ser ANTES de montar
     la cadena de RX: cambiar el canal reinicia la radio del driver y deja
     el PHY fuera del modo de trabajo, con lo que la captura sale con un
     valor constante en vez de señal. */
  uint8_t ch = 1;
  if (freq_mhz >= 2412 && freq_mhz <= 2472) {
    uint32_t offset = freq_mhz - 2407u;
    ch = (uint8_t) ((offset + 2u) / 5u);
    if (ch < 1)
      ch = 1;
    if (ch > 13)
      ch = 13;
  } else if (freq_mhz == 2484) {
    ch = 14;
  } else if (freq_mhz < 2412) {
    ch = 1;
  } else {
    ch = 13;
  }
  bool ch_changed = false;
  if (wifi_driver_get_channel() != ch) {
    wifi_driver_set_channel(ch);
    ch_changed = true;
  }

  /* Setup RX signal chain registers */
  pbus_workmode();
  rom_pbus_xpd_tx_off();
  ram_pbus_xpd_rx_on(1);
  set_rxclk_en(1);
  phy_force_rx_gain(s_gain_manual ? 1u : 0u, (unsigned) s_gain_index);

  /* El margen de asentamiento del upstream cuando cambia el canal. */
  if (ch_changed) {
    esp_rom_delay_us(3000);
  }
}

esp_err_t c6_sdr_rx_set_gain(bool manual, uint32_t gain_index) {
  if (gain_index > 79u) {
    gain_index = 79u;
  }
  s_gain_manual = manual;
  s_gain_index = gain_index;
  if (s_rx_active && !s_carrier_tx_active) {
    phy_force_rx_gain(s_gain_manual ? 1u : 0u, (unsigned) s_gain_index);
  }
  return ESP_OK;
}

esp_err_t c6_sdr_rx_tune(uint32_t freq_mhz) {
  if (freq_mhz < 100 || freq_mhz > 6000) {
    ESP_LOGE(TAG, "Invalid frequency: %lu MHz", (unsigned long) freq_mhz);
    return ESP_ERR_INVALID_ARG;
  }

  s_current_freq = freq_mhz;

  uint8_t ch = 1;
  if (freq_mhz >= 2412 && freq_mhz <= 2472) {
    uint32_t offset = freq_mhz - 2407u;
    ch = (uint8_t) ((offset + 2u) / 5u);
    if (ch < 1)
      ch = 1;
    if (ch > 13)
      ch = 13;
  } else if (freq_mhz == 2484) {
    ch = 14;
  } else if (freq_mhz < 2412) {
    ch = 1;
  } else {
    ch = 13;
  }

  if (s_carrier_tx_active) {
    if (wifi_driver_get_channel() != ch) {
      wifi_driver_set_channel(ch);
    }
    pbus_workmode();
    rom_pbus_xpd_rx_off();
    ram_pbus_xpd_tx_on(1);
    ram_stop_tx_tone(0);
  } else {
    rx_front_end(freq_mhz);
    esp_rom_delay_us(3000);
  }

  ESP_LOGI(TAG, "Tuned to %lu MHz (CH %d)%s", (unsigned long) freq_mhz, ch,
           s_carrier_tx_active ? " [TX CW]" : "");
  return ESP_OK;
}

uint32_t c6_sdr_rx_get_freq(void) {
  return s_current_freq;
}

bool c6_sdr_rx_capture(uint32_t num_samples, uint32_t* capture_us) {
  return c6_sdr_rx_capture_rate(num_samples, C6_SDR_RATE_80MSPS, capture_us);
}

/* On ESP32-C6, the hardware dump engine (0x600a9004) only captures at the
   native 80 MS/s rate; bits 0..16 of DUMP_CTRL_REG are the sample count
   (setting bits 15-16 overflows the 64 KB SRAM capture bank). Lower rates
   (40 MS/s and 16 MS/s) are decimated in software from the 80 MS/s capture. */
static bool decimation_step_for_rate(uint32_t rate, uint32_t* step) {
  switch (rate) {
    case C6_SDR_RATE_80MSPS:
      *step = 1u;
      return true;
    case C6_SDR_RATE_40MSPS:
      *step = 2u;
      return true;
    case C6_SDR_RATE_16MSPS:
      *step = 5u;
      return true;
    default:
      return false;
  }
}

bool c6_sdr_rx_capture_rate(uint32_t num_samples,
                            uint32_t rate,
                            uint32_t* capture_us) {
  uint32_t step = 1u;

  if (!s_rx_active) {
    if (c6_sdr_rx_init() != ESP_OK) {
      return false;
    }
  }

  if (!decimation_step_for_rate(rate, &step)) {
    ESP_LOGW(TAG, "Unsupported sample rate code %lu", (unsigned long) rate);
    return false;
  }

  if (num_samples < 256)
    num_samples = 256;
  if (num_samples > C6_SDR_MAX_SAMPLES)
    num_samples = C6_SDR_MAX_SAMPLES;

  uint32_t hw_samples = num_samples * step;
  if (hw_samples > C6_SDR_MAX_SAMPLES) {
    hw_samples = C6_SDR_MAX_SAMPLES;
  }

  /* Write canary test values to detect capture overrun and timeouts */
  for (uint32_t j = 0; j < hw_samples; j++) {
    s_iq_buf[j] = 0xa5a0055au;
  }
  for (uint32_t j = 0; j < 4; j++) {
    s_iq_buf[hw_samples + j] = 0x5a5aa5a5u ^ j;
  }

  /* La cadena de RX se rehace antes de cada captura: el driver de Wi-Fi
     puede haber movido el canal desde la última sintonización. */
  rx_front_end(s_current_freq);

  int64_t start_time = esp_timer_get_time();
  const uint32_t owner = REG_READ(SRAM_OWNER_REG);

  /* Force modem clocks for the RF dump engine */
  REG_WRITE(MODEM_CLK_REG1, 0xffffffffu);
  REG_WRITE(MODEM_CLK_REG2, 0x0007ffffu);
  REG_WRITE(MODEM_CLK_REG3, 0xffffffffu);
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(DUMP_CFG_REG1, (REG_READ(DUMP_CFG_REG1) & ~0x01ffffffu) |
                               (1u << 6) | (2u << 12) | (3u << 18));
  REG_WRITE(DUMP_CFG_REG2,
            (REG_READ(DUMP_CFG_REG2) & ~0x00078000u) | (15u << 15));
  REG_CLR_BIT(DUMP_FE_REG, 1);

  /* Grant SRAM bank owner to dump engine */
  REG_WRITE(SRAM_OWNER_REG, (owner & ~0xf00u) | 0x400u);
  __asm__ volatile("fence rw,rw" ::: "memory");
  (void) REG_READ(SRAM_OWNER_REG);

  /* Trigger capture at native 80 MS/s. Never set bits 15-16 on ESP32-C6. */
  const uint32_t count = hw_samples;
  REG_WRITE(DUMP_CTRL_REG, count | (1u << 18));
  REG_WRITE(DUMP_CTRL_REG, count | (1u << 31));
  REG_WRITE(DUMP_CTRL_REG, count | (1u << 31) | (1u << 19));
  REG_WRITE(DUMP_CTRL_REG, count | (1u << 31));

  const int64_t deadline = esp_timer_get_time() + 25000; /* 25 ms timeout */
  bool done = false;
  do {
    done = (REG_READ(DUMP_CTRL_REG) & (1u << 18)) != 0;
  } while (!done && esp_timer_get_time() < deadline);

  /* Reset dump controller and restore CPU ownership */
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(SRAM_OWNER_REG, owner);
  __asm__ volatile("fence rw,rw" ::: "memory");
  (void) REG_READ(SRAM_OWNER_REG);

  int64_t elapsed = esp_timer_get_time() - start_time;
  if (capture_us) {
    *capture_us = (uint32_t) elapsed;
  }

  if (!done) {
    ESP_LOGW(TAG, "Capture timeout (deadline exceeded)");
    return false;
  }

  /* Verify that data was written and canary was replaced */
  if (s_iq_buf[0] == 0xa5a0055au && s_iq_buf[1] == 0xa5a0055au) {
    ESP_LOGW(TAG, "Capture incomplete (buffer unchanged)");
    return false;
  }

  /* The four guard words sit right after the samples: if the engine wrote
     past the count it clobbered them, and the capture is short. */
  for (uint32_t j = 0; j < 4; j++) {
    if (s_iq_buf[hw_samples + j] != (0x5a5aa5a5u ^ j)) {
      ESP_LOGW(TAG, "Capture overrun (the engine wrote past the count)");
      return false;
    }
  }

  if (step > 1u) {
    for (uint32_t j = 0; j < num_samples; j++) {
      uint32_t src_idx = (j * step) % hw_samples;
      s_iq_buf[j] = s_iq_buf[src_idx];
    }
  }

  return true;
}

const uint32_t* c6_sdr_rx_get_buffer(void) {
  return (const uint32_t*) s_iq_buf;
}

void c6_sdr_rx_stop(void) {
  if (s_carrier_tx_active) {
    c6_sdr_tx_carrier_stop();
  }

  if (!s_rx_active) {
    return;
  }

  ESP_LOGI(TAG, "Stopping C6 native SDR RX engine...");
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(SRAM_OWNER_REG, s_saved_sram_owner);
  __asm__ volatile("fence rw,rw" ::: "memory");

  /* The claim this driver took is given back. The lease counts the claims,
     so the Wi-Fi stack only goes down when this was the last holder: the
     apps that were using it keep it. */
  radio_lease_prom_release(RADIO_PROM_SDR_RX);
  radio_lease_wifi_release();

  if (s_we_started_wifi) {
    ESP_LOGI(TAG,
             "The Wi-Fi driver was started for the capture and is free again");
  }
  s_rx_active = false;
}

bool c6_sdr_rx_is_active(void) {
  return s_rx_active;
}

esp_err_t c6_sdr_tx_carrier_start(void) {
  if (!s_rx_active) {
    esp_err_t err = c6_sdr_rx_init();
    if (err != ESP_OK) {
      return err;
    }
  }

  if (s_carrier_tx_active) {
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Starting PHY CW carrier tone on %lu MHz...",
           (unsigned long) s_current_freq);
  set_rxclk_en(0);
  rom_pbus_xpd_rx_off();
  pbus_workmode();
  ram_pbus_xpd_tx_on(1);
  ram_stop_tx_tone(0);
  s_carrier_tx_active = true;
  return ESP_OK;
}

void c6_sdr_tx_carrier_stop(void) {
  if (!s_carrier_tx_active) {
    return;
  }

  ESP_LOGI(TAG, "Stopping PHY CW carrier tone, restoring RX front-end...");
  ram_stop_tx_tone(1);
  rom_pbus_xpd_tx_off();
  pbus_workmode();
  ram_pbus_xpd_rx_on(1);
  set_rxclk_en(1);
  s_carrier_tx_active = false;
}

bool c6_sdr_tx_carrier_is_active(void) {
  return s_carrier_tx_active;
}
