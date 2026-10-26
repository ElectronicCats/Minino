/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The "host mode": Minino turns the USB serial port into the receiver the
 * ESP-WebSDR browser viewer drives.
 *
 * The viewer speaks the burst protocol of the ESP-SDR firmware; see
 * esdr_burst_proto.h for the protocol itself and esdr_host.h for the entry
 * point.
 *
 * Three things have to line up for the session to work:
 *
 *  - One reader for the port. The console task calls this from inside a
 *    console command, so it is between two reads and the USB serial JTAG
 *    driver ring buffer has nothing pending.
 *
 *  - A silent console. Every ESP_LOGx goes to the same USB port, and a log
 *    line in the middle of a binary capture corrupts it, so the master log
 *    level goes down to NONE for the duration of the session.
 *
 *  - A radio nobody else touches. The dump engine borrows the Wi-Fi PHY, so
 *    the driver takes a radio lease and only gives it back when the last
 *    holder lets go.
 */

#include "esdr_host.h"

#include "c6_sdr_rx.h"
#include "driver/usb_serial_jtag.h"
#include "esdr_burst_proto.h"
#include "esdr_spectrum.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char* TAG = "esdr_host";

/* The viewer gives up the receiver with RELEASE; a terminal user can send
   MININO, which is Minino's own escape. */
#define ESDR_IDLE_EXIT_MS     60000 /* no traffic at all: the session is over */
#define ESDR_READ_TIMEOUT_MS  100
#define ESDR_WRITE_TIMEOUT_MS 2000

static char s_line[ESDR_LINE_MAX];
static size_t s_used = 0;
static bool s_overflow = false;
static bool s_gain_manual = false;
static uint32_t s_gain_index = 0;

/* --------------------------------------------------------------------------
 * The port
 * -------------------------------------------------------------------------- */

/* El driver del USB Serial/JTAG mete la carga en un anillo de 256 bytes con
   xRingbufferSend(), y esa llamada no acepta un bloque mayor que el anillo:
   se queda esperando un hueco que no va a llegar y devuelve 0 sin escribir
   nada. Por eso la carga va a trozos del tamaño del FIFO del hardware. */
#define ESDR_TX_CHUNK 64

static void port_write(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*) data;
  size_t done = 0;

  while (done < len) {
    size_t chunk = len - done;
    if (chunk > ESDR_TX_CHUNK) {
      chunk = ESDR_TX_CHUNK;
    }
    int written = usb_serial_jtag_write_bytes(
        p + done, chunk, pdMS_TO_TICKS(ESDR_WRITE_TIMEOUT_MS));
    if (written <= 0) {
      /* The host is not reading or the port went away: there is nothing to
         do but give up on this session. */
      return;
    }
    done += (size_t) written;
  }
}

static void port_line(const char* text) {
  port_write(text, strlen(text));
}

static void port_reply(const char* buffer, size_t len) {
  if (len == 0) {
    port_line("ERR reply\n");
    return;
  }
  port_write(buffer, len);
}

/* --------------------------------------------------------------------------
 * The captures
 * -------------------------------------------------------------------------- */

/**
 * @brief Capture a burst and send it the way the viewer expects it.
 *
 * The payload is packed in place over the raw words of the capture bank and
 * the CRC covers the packed bytes, which is what the viewer checks.
 */
static void send_capture(uint32_t samples, esdr_format_t format) {
  uint32_t capture_us = 0;
  char reply[ESDR_REPLY_MAX];

  if (samples < ESDR_MIN_SAMPLES) {
    samples = ESDR_MIN_SAMPLES;
  }
  if (samples > ESDR_MAX_SAMPLES) {
    samples = ESDR_MAX_SAMPLES;
  }

  if (!c6_sdr_rx_capture(samples, &capture_us)) {
    port_line("ERR capture_timeout\n");
    return;
  }

  uint32_t* words = (uint32_t*) c6_sdr_rx_get_buffer();
  size_t bytes = 0;
  if (format == ESDR_FORMAT_IQ8) {
    bytes = esdr_pack_iq8(words, samples);
  } else {
    bytes = esdr_pack_iq10(words, samples);
  }

  uint32_t crc = esdr_crc32((const uint8_t*) words, bytes);
  if (esdr_reply_data(reply, sizeof(reply), samples, crc, capture_us) == 0) {
    port_line("ERR reply\n");
    return;
  }
  port_write(reply, strlen(reply));
  port_write(words, bytes);
}

/* --------------------------------------------------------------------------
 * El transporte
 *
 * Lo usan tambien los modulos que hablan el mismo protocolo (el de los
 * espectros), asi que vive aqui y no en el modulo de espectro.
 * -------------------------------------------------------------------------- */

static int64_t s_session_start = 0;
static volatile bool s_user_stop_requested = false;
static volatile bool s_host_active = false;

void esdr_host_request_stop(void) {
  s_user_stop_requested = true;
}

bool esdr_host_is_active(void) {
  return s_host_active;
}

void esdr_host_write(const void* data, size_t len) {
  port_write(data, len);
}

bool esdr_host_stop_requested(void) {
  if (s_user_stop_requested) {
    return true;
  }
  uint8_t byte;
  /* El visor corta la corriente mandando una linea vacia. */
  while (usb_serial_jtag_read_bytes(&byte, 1, 0) == 1) {
    if (byte == '\n') {
      return true;
    }
  }
  return false;
}

void esdr_host_yield(void) {
  /* Cederle el paso a la tarea idle es lo que alimenta el watchdog: sin
     esto, un bucle largo de capturas lo dispara. */
  vTaskDelay(1);
}

uint64_t esdr_host_elapsed_ms(void) {
  if (s_session_start == 0) {
    s_session_start = esp_timer_get_time();
  }
  return (uint64_t) ((esp_timer_get_time() - s_session_start) / 1000);
}

/* --------------------------------------------------------------------------
 * The command dispatcher
 * -------------------------------------------------------------------------- */

static bool rate_is_supported(uint32_t rate) {
  return rate == ESDR_RATE_80MSPS || rate == ESDR_RATE_40MSPS ||
         rate == ESDR_RATE_16MSPS;
}

/* La captura que pide el modo de espectros: el mismo motor de captura, con
   las palabras crudas en el banco. */
static bool spectrum_acquire(uint32_t n,
                             uint32_t rate,
                             const uint32_t** words,
                             uint32_t* elapsed_us) {
  uint32_t capture_us = 0;
  if (!c6_sdr_rx_capture_rate(n, rate, &capture_us)) {
    return false;
  }
  *words = c6_sdr_rx_get_buffer();
  if (elapsed_us) {
    *elapsed_us = capture_us;
  }
  return true;
}

static void handle_command(const esdr_request_t* req) {
  char reply[ESDR_REPLY_MAX];

  switch (req->cmd) {
    case ESDR_CMD_SYNC:
      esdr_reply_sync(reply, sizeof(reply), req->nonce);
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_INFO:
      esdr_reply_info(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_CAPS:
      esdr_reply_caps(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_VERSION:
      esdr_reply_version(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_LIMITS:
      esdr_reply_limits(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_RANGE:
      esdr_reply_range(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_TRANSPORT:
      esdr_reply_transport(reply, sizeof(reply));
      port_reply(reply, strlen(reply));
      break;

    case ESDR_CMD_GAIN:
      if (req->gain_query) {
        esdr_reply_gain(reply, sizeof(reply), s_gain_manual, s_gain_index);
        port_reply(reply, strlen(reply));
      } else {
        s_gain_manual = req->gain_manual;
        s_gain_index = req->gain_index;
        port_line("OK\n");
      }
      break;

    case ESDR_CMD_FREQ:
      if (req->freq_mhz < ESDR_FREQ_MIN_MHZ ||
          req->freq_mhz > ESDR_FREQ_MAX_MHZ) {
        port_line("ERR frequency\n");
        break;
      }
      if (c6_sdr_rx_tune(req->freq_mhz) != ESP_OK) {
        port_line("ERR frequency\n");
        break;
      }
      port_line("OK\n");
      break;

    case ESDR_CMD_BANDWIDTH:
      /* The analog filter is not implemented, so the requested bandwidth
         cannot be honoured. */
      port_line("ERR bandwidth\n");
      break;

    case ESDR_CMD_GPIO:
      port_line("ERR gpio\n");
      break;

    case ESDR_CMD_CAP16:
    case ESDR_CMD_CAP20:
      if (!rate_is_supported(req->rate)) {
        port_line("ERR rate\n");
        break;
      }
      if (req->samples < ESDR_MIN_SAMPLES || req->samples > ESDR_MAX_SAMPLES) {
        port_line("ERR samples\n");
        break;
      }
      send_capture(req->samples, (esdr_format_t) req->format);
      break;

    case ESDR_CMD_RXRUN: {
      if (!rate_is_supported(req->rate)) {
        port_line("ERR rate\n");
        break;
      }
      if (req->samples < ESDR_MIN_SAMPLES || req->samples > ESDR_MAX_SAMPLES ||
          req->repeats == 0 || req->repeats > 1000 ||
          (req->format != ESDR_FORMAT_IQ8 && req->format != ESDR_FORMAT_IQ10)) {
        port_line("ERR samples\n");
        break;
      }
      for (uint32_t j = 0; j < req->repeats; j++) {
        send_capture(req->samples, (esdr_format_t) req->format);
      }
      port_line("END\n");
      break;
    }

    case ESDR_CMD_RELEASE:
    case ESDR_CMD_MININO:
      port_line("OK\n");
      break;

    case ESDR_CMD_BAUD:
      port_line("ERR baud_transport\n");
      break;

    case ESDR_CMD_SPEC:
      /* El modo de espectros: la placa calcula la FFT y manda los codigos
         de potencia. El comando se queda aqui hasta que el cliente lo
         corta. */
      esdr_spectrum_command(req->args, c6_sdr_rx_get_freq(), spectrum_acquire);
      break;

    case ESDR_CMD_UNKNOWN:
    default:
      port_line("ERR command\n");
      break;
  }
}

/* --------------------------------------------------------------------------
 * The session
 * -------------------------------------------------------------------------- */

/**
 * @brief Read one line from the port.
 *
 * @return 1 when a line is ready (an empty line included), 0 when nothing
 *         arrived, -1 on an over-long line.
 */
static int poll_line(void) {
  uint8_t byte;

  for (unsigned k = 0; k < ESDR_LINE_MAX; k++) {
    int n = usb_serial_jtag_read_bytes(&byte, 1,
                                       pdMS_TO_TICKS(ESDR_READ_TIMEOUT_MS));
    if (n != 1) {
      return 0;
    }
    if (byte == '\r') {
      continue;
    }
    if (byte != '\n') {
      if (s_used < ESDR_LINE_MAX - 1) {
        s_line[s_used++] = (char) byte;
      } else {
        s_overflow = true;
      }
      continue;
    }

    size_t used = s_used;
    bool overflow = s_overflow;
    s_used = 0;
    s_overflow = false;

    if (overflow) {
      s_line[0] = '\0';
      return -1;
    }
    s_line[used] = '\0';
    return 1;
  }
  return -1;
}

esp_err_t esdr_host_run(void) {
  esp_err_t ret = ESP_OK;
  s_user_stop_requested = false;
  s_host_active = true;
  int64_t last_command = esp_timer_get_time();
  int64_t last_traffic = last_command;

  /* The radio comes up BEFORE the logs are silenced: the Wi-Fi driver
     prints from its own ROM, which does not go through esp_log and cannot
     be silenced with the master level. Bringing it up here keeps that
     noise out of the middle of a binary capture. */
  if (c6_sdr_rx_init() != ESP_OK) {
    ESP_LOGE(TAG, "The radio did not come up: the SDR host mode cannot start");
    s_host_active = false;
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "SDR host mode: serving the burst protocol on the USB port");

  /* Nothing may reach the port while a binary capture is in flight. The
     master level is used because the per tag levels would survive a change
     of the default one. */
  esp_log_level_t saved_master = esp_log_get_level_master();
  esp_log_set_level_master(ESP_LOG_NONE);

  s_used = 0;
  s_overflow = false;
  s_line[0] = '\0';

  for (;;) {
    if (s_user_stop_requested) {
      break;
    }
    int status = poll_line();
    int64_t now = esp_timer_get_time();

    if (status != 0) {
      last_traffic = now;
    }

    if (status < 0) {
      port_line("ERR command_length\n");
      last_command = now;
    } else if (status > 0 && s_line[0] != '\0') {
      esdr_request_t req;
      if (!esdr_parse_line(s_line, &req)) {
        port_line("ERR command\n");
      } else {
        handle_command(&req);
        if (req.cmd == ESDR_CMD_RELEASE || req.cmd == ESDR_CMD_MININO) {
          last_command = now;
          break;
        }
      }
      last_command = now;
    }

    /* The session owns the radio from here to the end: releasing it and
       taking it back later would make the Wi-Fi driver print its own
       startup noise in the middle of the stream, and the browser would
       read that noise as a broken capture. */
    if ((now - last_traffic) > (int64_t) ESDR_IDLE_EXIT_MS * 1000) {
      break;
    }
  }

  c6_sdr_rx_stop();

  esp_log_set_level_master(saved_master);
  s_host_active = false;
  s_user_stop_requested = false;
  return ret;
}
