/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Screen rendering for Minino Native SDR on SSD1306/SH1106 OLED (128x64).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SDR_MODE_RX_CAP = 0,
  SDR_MODE_TX_PKT = 1,
  SDR_MODE_TX_CW  = 2
} sdr_operating_mode_t;

typedef struct {
  uint32_t freq_mhz;
  uint32_t sample_rate_hz;
  uint32_t burst_count;
  int8_t   rssi_est;
  bool     streaming;
  sdr_operating_mode_t mode;
  bool     tx_active;
  uint32_t tx_count;
  char     status_line[24];
  char     footer_line[24];
} sdr_screen_data_t;

/**
 * @brief Render current SDR status on the OLED screen.
 */
void sdr_screen_render(const sdr_screen_data_t* data);

#ifdef __cplusplus
}
#endif
