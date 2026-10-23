#include "esp_sdr_module.h"
#include "app_manager.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "c6_sdr_rx.h"
#include "files_ops.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "flash_fs.h"
#include <math.h>
#include "esp_wifi.h"
#include "esp_rom_sys.h"
#include "sd_card.h"
#include "sdr_screen.h"

static const char* TAG = "esp_sdr_module";

static void esp_sdr_start(void);
static void sdr_input_handler(uint8_t button_name, uint8_t button_event);

static const minino_app_t s_app = {
    .name = "esp_sdr",
    .on_enter = esp_sdr_start,
    .on_input = sdr_input_handler,
    .on_exit = esp_sdr_module_stop,
};

#define SDR_BURST_SAMPLES  8192
#define SDR_FREQ_MIN_MHZ   2412u
#define SDR_FREQ_MAX_MHZ   2484u

typedef struct {
  bool                 running;
  uint32_t             freq_mhz;
  uint32_t             burst_count;
  uint32_t             last_us;
  uint32_t             last_samples;
  int8_t               last_rssi;
  sdr_operating_mode_t mode;
  bool                 tx_active;
  uint32_t             tx_count;
} sdr_native_state_t;

static sdr_native_state_t s_sdr = {
    .running = false,
    .freq_mhz = SDR_FREQ_MIN_MHZ,
    .last_rssi = -100,
    .mode = SDR_MODE_RX_CAP,
    .tx_active = false,
    .tx_count = 0,
};

static esp_timer_handle_t s_carrier_timer = NULL;
static void sdr_render_screen(const char* status_line, const char* footer_line);

static void carrier_timeout_cb(void* arg) {
  if (s_sdr.running && s_sdr.tx_active && s_sdr.mode == SDR_MODE_TX_CW) {
    ESP_LOGW(TAG, "Carrier emission safety timeout reached (30s). Stopping TX CW...");
    c6_sdr_tx_carrier_stop();
    s_sdr.tx_active = false;
    sdr_render_screen("CW: TIMEOUT (OFF)", NULL);
  }
}

static uint32_t sdr_freq_next_up(uint32_t freq) {
  if (freq >= 2412 && freq < 2472) {
    return freq + 5;
  }
  if (freq == 2472) {
    return 2484;
  }
  return 2484;
}

static uint32_t sdr_freq_next_down(uint32_t freq) {
  if (freq == 2484) {
    return 2472;
  }
  if (freq > 2412) {
    return freq - 5;
  }
  return 2412;
}

static int8_t compute_iq_rssi(const uint32_t* words, uint32_t num_samples) {
  if (words == NULL || num_samples == 0) {
    return -100;
  }
  uint64_t sum_pwr = 0;
  for (uint32_t i = 0; i < num_samples; i++) {
    uint32_t w = words[i];
    int32_t re = ((int32_t)(w << 22)) >> 22;
    int32_t im = ((int32_t)((w >> 10) << 22)) >> 22;
    sum_pwr += (uint64_t)(re * re + im * im);
  }
  double avg_pwr = (double)sum_pwr / (double)num_samples;
  if (avg_pwr < 1.0) {
    return -100;
  }
  /* Full scale is 524288 (512^2 + 512^2) */
  double dbfs = 10.0 * log10(avg_pwr / 524288.0);
  int8_t est_rssi = (int8_t)(dbfs - 25.0);
  if (est_rssi > -20) est_rssi = -20;
  if (est_rssi < -100) est_rssi = -100;
  return est_rssi;
}

static void sdr_render_screen(const char* status_line, const char* footer_line) {
  sdr_screen_data_t view = {
      .freq_mhz = s_sdr.freq_mhz,
      .sample_rate_hz = 80000000u,
      .burst_count = s_sdr.burst_count,
      .rssi_est = s_sdr.last_rssi,
      .streaming = false,
      .mode = s_sdr.mode,
      .tx_active = s_sdr.tx_active,
      .tx_count = s_sdr.tx_count,
  };
  if (status_line) {
    strncpy(view.status_line, status_line, sizeof(view.status_line) - 1);
    view.status_line[sizeof(view.status_line) - 1] = '\0';
  } else {
    view.status_line[0] = '\0';
  }
  if (footer_line) {
    strncpy(view.footer_line, footer_line, sizeof(view.footer_line) - 1);
    view.footer_line[sizeof(view.footer_line) - 1] = '\0';
  } else {
    view.footer_line[0] = '\0';
  }
  sdr_screen_render(&view);
}

static void sdr_tx_packet_run(void) {
  uint8_t ch = 1;
  if (s_sdr.freq_mhz >= 2412 && s_sdr.freq_mhz <= 2472) {
    ch = (uint8_t)(((s_sdr.freq_mhz - 2407u) + 2u) / 5u);
  } else if (s_sdr.freq_mhz == 2484) {
    ch = 14;
  }

  uint8_t frame[128];
  size_t len = 0;

  /* Frame Control: Probe Request (0x0040) */
  frame[len++] = 0x40;
  frame[len++] = 0x00;
  /* Duration */
  frame[len++] = 0x00;
  frame[len++] = 0x00;
  /* Destination Address: Broadcast */
  memset(&frame[len], 0xff, 6);
  len += 6;
  /* Source Address: Locally Administered */
  frame[len++] = 0x02;
  frame[len++] = 0xec;
  frame[len++] = 0xc6;
  frame[len++] = (uint8_t)(s_sdr.tx_count >> 16);
  frame[len++] = (uint8_t)(s_sdr.tx_count >> 8);
  frame[len++] = (uint8_t)(s_sdr.tx_count);
  /* BSSID: Broadcast */
  memset(&frame[len], 0xff, 6);
  len += 6;
  /* Sequence Control */
  frame[len++] = (uint8_t)((s_sdr.tx_count & 0x0f) << 4);
  frame[len++] = (uint8_t)((s_sdr.tx_count >> 4) & 0xff);

  /* SSID Tag */
  const char* ssid = "MININO-SDR-TX";
  uint8_t ssid_len = (uint8_t)strlen(ssid);
  frame[len++] = 0x00;
  frame[len++] = ssid_len;
  memcpy(&frame[len], ssid, ssid_len);
  len += ssid_len;

  /* Supported Rates */
  frame[len++] = 0x01;
  frame[len++] = 8;
  frame[len++] = 0x82;
  frame[len++] = 0x84;
  frame[len++] = 0x8b;
  frame[len++] = 0x96;
  frame[len++] = 0x0c;
  frame[len++] = 0x12;
  frame[len++] = 0x18;
  frame[len++] = 0x24;

  /* DS Parameter Set (Channel) */
  frame[len++] = 0x03;
  frame[len++] = 1;
  frame[len++] = ch;

  s_sdr.tx_active = true;
  for (int i = 0; i < 5; i++) {
    (void)esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
    s_sdr.tx_count++;
    esp_rom_delay_us(1000);
  }
  s_sdr.tx_active = false;

  char status[24];
  snprintf(status, sizeof(status), "SENT %lu pkts (C%d)", (unsigned long)s_sdr.tx_count, (int)ch);
  sdr_render_screen(status, NULL);
}

static const char* sdr_burst_save(void) {
  const char* base_dir = NULL;
  const char* dest_name = NULL;

  if (sd_card_is_mounted()) {
    base_dir = SD_CARD_PATH;
    dest_name = "SD";
  } else if (flash_fs_is_mounted() || flash_fs_mount() == ESP_OK) {
    base_dir = "/internal";
    dest_name = "Flash";
  } else {
    return NULL;
  }

  char filename[64];
  if (!files_ops_incremental_name_buf(base_dir, "sdrcap", ".iq", filename, sizeof(filename))) {
    return NULL;
  }

  FILE* file = fopen(filename, "wb");
  if (file == NULL) {
    return NULL;
  }

  const uint32_t* words = c6_sdr_rx_get_buffer();
  size_t written = fwrite(words, sizeof(uint32_t), s_sdr.last_samples, file);
  fclose(file);

  if (written != s_sdr.last_samples) {
    remove(filename);
    return NULL;
  }

  return dest_name;
}

static void sdr_burst_run(void) {
  uint32_t capture_us = 0;
  if (!c6_sdr_rx_capture(SDR_BURST_SAMPLES, &capture_us)) {
    ESP_LOGW(TAG, "burst capture failed");
    sdr_render_screen("CAP FAIL", "LEFT: exit");
    return;
  }
  s_sdr.last_us = capture_us;
  s_sdr.last_samples = SDR_BURST_SAMPLES;
  s_sdr.burst_count++;

  /* Compute live physical RSSI from raw I/Q samples */
  const uint32_t* words = c6_sdr_rx_get_buffer();
  s_sdr.last_rssi = compute_iq_rssi(words, s_sdr.last_samples);

  const char* dest = sdr_burst_save();
  char status[24];
  char footer[24];

  snprintf(status, sizeof(status), "CAP %lu smp %lu us",
           (unsigned long) s_sdr.last_samples, (unsigned long) s_sdr.last_us);

  if (dest != NULL) {
    snprintf(footer, sizeof(footer), "Saved to %s", dest);
  } else {
    snprintf(footer, sizeof(footer), "Cap OK: not saved");
  }

  sdr_render_screen(status, footer);

  ESP_LOGI(TAG, "burst %lu: %lu words in %lu us (est RSSI %d dBm)%s%s",
           (unsigned long) s_sdr.burst_count,
           (unsigned long) s_sdr.last_samples,
           (unsigned long) s_sdr.last_us,
           (int) s_sdr.last_rssi,
           dest ? " saved to " : " (not saved)",
           dest ? dest : "");
}

static void sdr_input_handler(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      ESP_LOGI(TAG, "Exiting native SDR app");
      if (s_sdr.tx_active && s_sdr.mode == SDR_MODE_TX_CW) {
        c6_sdr_tx_carrier_stop();
        s_sdr.tx_active = false;
      }
      if (s_carrier_timer != NULL) {
        esp_timer_stop(s_carrier_timer);
      }
      app_manager_exit();
      break;
    case BUTTON_BOOT:
      if (s_sdr.mode == SDR_MODE_TX_CW && s_sdr.tx_active) {
        c6_sdr_tx_carrier_stop();
        s_sdr.tx_active = false;
        if (s_carrier_timer != NULL) {
          esp_timer_stop(s_carrier_timer);
        }
      }
      s_sdr.mode = (sdr_operating_mode_t)((s_sdr.mode + 1) % 3);
      sdr_render_screen(NULL, NULL);
      break;
    case BUTTON_RIGHT:
      if (s_sdr.mode == SDR_MODE_RX_CAP) {
        sdr_burst_run();
      } else if (s_sdr.mode == SDR_MODE_TX_PKT) {
        sdr_tx_packet_run();
      } else if (s_sdr.mode == SDR_MODE_TX_CW) {
        if (s_sdr.tx_active) {
          c6_sdr_tx_carrier_stop();
          s_sdr.tx_active = false;
          if (s_carrier_timer != NULL) {
            esp_timer_stop(s_carrier_timer);
          }
          sdr_render_screen("CW: OFF (STANDBY)", NULL);
        } else {
          esp_err_t err = c6_sdr_tx_carrier_start();
          if (err == ESP_OK) {
            s_sdr.tx_active = true;
            if (s_carrier_timer == NULL) {
              const esp_timer_create_args_t timer_args = {
                  .callback = carrier_timeout_cb,
                  .name = "sdr_cw_timeout",
              };
              (void)esp_timer_create(&timer_args, &s_carrier_timer);
            }
            if (s_carrier_timer != NULL) {
              esp_timer_stop(s_carrier_timer);
              esp_timer_start_once(s_carrier_timer, 30000000); /* 30 seconds limit */
            }
            sdr_render_screen("CW: ON (30s limit)", NULL);
          } else {
            sdr_render_screen("CW: ERR START", NULL);
          }
        }
      }
      break;
    case BUTTON_UP:
      if (s_sdr.freq_mhz < SDR_FREQ_MAX_MHZ) {
        s_sdr.freq_mhz = sdr_freq_next_up(s_sdr.freq_mhz);
        (void) c6_sdr_rx_tune(s_sdr.freq_mhz);
        sdr_render_screen(NULL, NULL);
      }
      break;
    case BUTTON_DOWN:
      if (s_sdr.freq_mhz > SDR_FREQ_MIN_MHZ) {
        s_sdr.freq_mhz = sdr_freq_next_down(s_sdr.freq_mhz);
        (void) c6_sdr_rx_tune(s_sdr.freq_mhz);
        sdr_render_screen(NULL, NULL);
      }
      break;
    default:
      break;
  }
}

static void esp_sdr_start(void) {
  ESP_LOGI(TAG, "Entering native SDR I/Q capture app...");
  s_sdr.running = true;
  s_sdr.last_samples = 0;
  s_sdr.last_us = 0;
  s_sdr.burst_count = 0;
  s_sdr.tx_count = 0;
  s_sdr.tx_active = false;
  s_sdr.mode = SDR_MODE_RX_CAP;

  /* Mount Flash FS for saving captures to internal storage. */
  if (!flash_fs_is_mounted()) {
    esp_err_t err = flash_fs_mount();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Internal flash not available (%s): captures not saved",
               esp_err_to_name(err));
    }
  }

  /* Initialize the radio: this sets up the WiFi PHY and SPI bus
     for the dump engine. */
  esp_err_t err = c6_sdr_rx_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SDR radio: %s", esp_err_to_name(err));
    app_manager_exit();
    return;
  }

  sdr_render_screen(NULL, NULL);
  menus_module_enable_input();
  sdr_burst_run();
}

void esp_sdr_module_begin(void) {
  app_manager_enter(&s_app);
}

void esp_sdr_module_stop(void) {
  if (s_carrier_timer != NULL) {
    esp_timer_stop(s_carrier_timer);
    esp_timer_delete(s_carrier_timer);
    s_carrier_timer = NULL;
  }
  if (s_sdr.running) {
    if (s_sdr.tx_active && s_sdr.mode == SDR_MODE_TX_CW) {
      c6_sdr_tx_carrier_stop();
      s_sdr.tx_active = false;
    }
    s_sdr.running = false;
    c6_sdr_rx_stop();
  }
  // Flash FS is managed globally, do not unmount here
}

bool esp_sdr_module_is_running(void) {
  return s_sdr.running;
}

void app_manager_enter_wrapper_esp_sdr(void) {
  extern const minino_app_t s_app;
  app_manager_enter(&s_app);
}

