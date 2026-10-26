/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The burst protocol of the ESP-SDR firmware, as the ESP-WebSDR viewer
 * speaks it. See esdr_burst_proto.h for the background.
 */

#include "esdr_burst_proto.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * The command parser
 *
 * The upstream firmware uses sscanf with a trailing " %c" to reject a line
 * that carries anything else after the numbers. The same trick is used here
 * so that "FREQ 2412 x" is rejected the way the firmware rejects it.
 * -------------------------------------------------------------------------- */

bool esdr_parse_line(const char* line, esdr_request_t* out) {
  if (line == NULL || out == NULL) {
    return false;
  }

  memset(out, 0, sizeof(*out));
  out->cmd = ESDR_CMD_UNKNOWN;

  /* The scanf targets are plain unsigned: on the 32 bit targets uint32_t is
     "long unsigned int" and the compiler rejects %u for it. */
  unsigned n = 0;
  unsigned rate = 0;
  unsigned repeats = 0;
  unsigned format = 0;
  uint64_t nonce = 0;
  char extra = 0;

  if (sscanf(line, "SYNC %" SCNu64 " %c", &nonce, &extra) == 1) {
    out->cmd = ESDR_CMD_SYNC;
    out->nonce = nonce;
    return true;
  }

  if (!strcmp(line, "INFO")) {
    out->cmd = ESDR_CMD_INFO;
    return true;
  }
  if (!strcmp(line, "CAPS")) {
    out->cmd = ESDR_CMD_CAPS;
    return true;
  }
  if (!strcmp(line, "VERSION?")) {
    out->cmd = ESDR_CMD_VERSION;
    return true;
  }
  if (!strcmp(line, "TRANSPORT?")) {
    out->cmd = ESDR_CMD_TRANSPORT;
    return true;
  }
  if (!strcmp(line, "RELEASE")) {
    out->cmd = ESDR_CMD_RELEASE;
    return true;
  }
  if (!strcmp(line, "RANGE?")) {
    out->cmd = ESDR_CMD_RANGE;
    return true;
  }
  if (!strcmp(line, "LIMITS?")) {
    out->cmd = ESDR_CMD_LIMITS;
    return true;
  }
  if (!strcmp(line, "MININO")) {
    /* Minino's own escape: the viewer never sends it, a terminal user can. */
    out->cmd = ESDR_CMD_MININO;
    return true;
  }

  if (!strcmp(line, "BAUD?") || !strncmp(line, "BAUD ", 5)) {
    /* El visor ofrece cambiar la velocidad del puerto. Esta sesion va por
       USB, donde la velocidad no existe: se responde como el firmware del
       ESPARGOS cuando el transporte no es el UART. */
    out->cmd = ESDR_CMD_BAUD;
    return true;
  }

  if (!strncmp(line, "SPEC", 4)) {
    /* El modo de espectros lleva demasiados argumentos para analizarlos
       aqui: se pasa la linea entera al modulo que lo implementa. Tamien
       entran aqui el SPECINFO? y el DC?. */
    out->cmd = ESDR_CMD_SPEC;
    snprintf(out->args, sizeof(out->args), "%s", line);
    return true;
  }

  if (sscanf(line, "FREQ %u %c", &n, &extra) == 1) {
    out->cmd = ESDR_CMD_FREQ;
    out->freq_mhz = n;
    return true;
  }

  if (sscanf(line, "GAIN MANUAL %u %c", &n, &extra) == 1) {
    out->cmd = ESDR_CMD_GAIN;
    out->gain_manual = true;
    out->gain_index = n;
    return true;
  }
  if (!strcmp(line, "GAIN HARDWARE")) {
    out->cmd = ESDR_CMD_GAIN;
    out->gain_manual = false;
    return true;
  }
  if (!strcmp(line, "GAIN?")) {
    out->cmd = ESDR_CMD_GAIN;
    return true;
  }

  if (sscanf(line, "BANDWIDTH %u %c", &n, &extra) == 1) {
    out->cmd = ESDR_CMD_BANDWIDTH;
    out->freq_mhz = n;
    return true;
  }

  if (!strncmp(line, "GPIO ", 5)) {
    out->cmd = ESDR_CMD_GPIO;
    return true;
  }
  if (!strcmp(line, "GPIO?")) {
    out->cmd = ESDR_CMD_GPIO;
    return true;
  }

  if (sscanf(line, "RXRUN %u %u %u %u %c", &n, &rate, &repeats, &format, &extra) == 4) {
    out->cmd = ESDR_CMD_RXRUN;
    out->samples = n;
    out->rate = rate;
    out->repeats = repeats;
    out->format = format;
    return true;
  }

  if (sscanf(line, "CAP16 %u %u %c", &n, &rate, &extra) == 2) {
    out->cmd = ESDR_CMD_CAP16;
    out->samples = n;
    out->rate = rate;
    out->format = ESDR_FORMAT_IQ8;
    return true;
  }
  if (sscanf(line, "CAP20 %u %u %c", &n, &rate, &extra) == 2) {
    out->cmd = ESDR_CMD_CAP20;
    out->samples = n;
    out->rate = rate;
    out->format = ESDR_FORMAT_IQ10;
    return true;
  }

  return false;
}

/* --------------------------------------------------------------------------
 * The sample formats
 *
 * The dump engine writes one 32 bit word per complex sample:
 *
 *   31        28 27        20 19        10 9         0
 *   +-----------+------------+------------+------------+
 *   |  unused   | gain index |     Q      |     I      |
 *   +-----------+------------+------------+------------+
 *
 * I and Q are signed ten bit values. The viewer wants them either as eight
 * bit pairs (I then Q) or packed twenty bits per pair, least significant bit
 * first, with I in bits 0-9 and Q in bits 10-19.
 * -------------------------------------------------------------------------- */

size_t esdr_pack_iq8(uint32_t* words, size_t count) {
  uint8_t* out = (uint8_t*)words;

  for (size_t j = 0; j < count; j++) {
    uint32_t w = words[j];

    /* I lives in bits 9-0 and Q in bits 19-10; the viewer wants the upper
       eight bits of each ten bit field. */
    out[2 * j] = (uint8_t)((w >> 2) & 0xffu);
    out[2 * j + 1] = (uint8_t)((w >> 12) & 0xffu);
  }
  return 2 * count;
}

size_t esdr_pack_iq10(uint32_t* words, size_t count) {
  uint8_t* out = (uint8_t*)words;

  /* Two samples in five bytes. The twenty bits of the dump word are already
     laid out as I in the low ten bits and Q in the high ten bits, which is
     the order the viewer reads, so the whole group goes out as it is. */
  for (size_t j = 0; j < count; j += 2) {
    uint32_t a = words[j] & 0xfffffu;
    uint32_t b = (j + 1 < count) ? (words[j + 1] & 0xfffffu) : 0u;

    out[0] = (uint8_t)a;
    out[1] = (uint8_t)(a >> 8);
    out[2] = (uint8_t)((a >> 16) | (b << 4));
    if (j + 1 < count) {
      out[3] = (uint8_t)(b >> 4);
      out[4] = (uint8_t)(b >> 12);
    }
    out += 5;
  }
  return (count * 20u + 7u) / 8u;
}

size_t esdr_wire_size(size_t count, esdr_format_t format) {
  if (format == ESDR_FORMAT_IQ8) {
    return count * 2u;
  }
  return (count * 20u + 7u) / 8u;
}

/* --------------------------------------------------------------------------
 * The CRC32 the viewer checks the payload with
 *
 * The same polynomial and conventions as zlib: reflected, 0xEDB88320, the
 * initial and final values complemented. A nibble at a time keeps the table
 * at sixteen entries, which matters in a build that is fighting for every
 * byte of heap.
 * -------------------------------------------------------------------------- */

static const uint32_t s_crc_nibble[16] = {
    0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
    0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
    0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
    0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};

uint32_t esdr_crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xffffffffu;

  for (size_t j = 0; j < len; j++) {
    crc ^= data[j];
    crc = (crc >> 4) ^ s_crc_nibble[crc & 0x0fu];
    crc = (crc >> 4) ^ s_crc_nibble[crc & 0x0fu];
  }
  return crc ^ 0xffffffffu;
}

/* --------------------------------------------------------------------------
 * The replies
 * -------------------------------------------------------------------------- */

static size_t emit(char* out, size_t len, const char* text) {
  size_t n = strlen(text);
  if (out == NULL || len < n + 1) {
    return 0;
  }
  memcpy(out, text, n);
  out[n] = '\0';
  return n;
}

size_t esdr_reply_info(char* out, size_t len) {
  char text[64];
  snprintf(text, sizeof(text), "%s %d burst %" PRIu32 "\n", ESDR_FAMILY_ID, ESDR_PROTOCOL_VERSION,
           (uint32_t)ESDR_MAX_SAMPLES);
  return emit(out, len, text);
}

size_t esdr_reply_caps(char* out, size_t len) {
  /* The flags the viewer reads:
     VERSION   the firmware answers VERSION?
     RXLIMITS  the firmware answers LIMITS?
     HWAGC     the receiver runs the hardware AGC
     IQ8       the 8 bit capture format is available
     SERIALLEASE  a client owns the receiver until RELEASE
     TUNEEXT   the tuning range is wider than the 2.4 GHz band and RANGE?
               reports it
     RX40      40 MS/s is available
     RX16      16 MS/s is available
     UARTBAUD  the firmware answers TRANSPORT? (the viewer needs it to know
               that it is on a USB link, and with it the on chip spectra)
     SPEC      the firmware computes the spectra on the chip
     SPECN     the viewer may ask for a given number of FFT bins
     SPECCAPS  the firmware answers SPECINFO? with the profiles
     DCT       the firmware corrects the DC offset of each FFT

     The flags that are not listed are the ones Minino does not implement:
     GPIO, GAIN, ALPF, LPF and SPECSTAT (the statistics records). The viewer
     hides its controls for the capabilities that are missing. */
  return emit(out, len,
              "CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16 UARTBAUD SPEC SPECN "
              "SPECCAPS DCT\n");
}

/* The compiler hands the build date over as "Mmm dd yyyy"; the viewer wants
   the ISO form, and wants the date of the build to be the date part of the
   timestamp. */
static void build_date_iso(char* out, size_t len) {
  static const char* months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char* date = __DATE__;
  int month = 0;

  for (int j = 0; j < 12; j++) {
    if (date[0] == months[j][0] && date[1] == months[j][1] && date[2] == months[j][2]) {
      month = j + 1;
      break;
    }
  }
  /* "__DATE__" is "Mmm dd yyyy": the month is 0-2, the day starts at 4 (it is
     blank padded) and the year is 7-10. */
  snprintf(out, len, "%c%c%c%c-%02d-%02d", date[7], date[8], date[9], date[10], month,
           (int)strtol(&date[4], NULL, 10));
}

size_t esdr_reply_version(char* out, size_t len) {
  char date[16];
  char text[160];

  /* The viewer parses this as JSON and compares the profile against the chip
     it thinks it is talking to, so the profile has to be "esp32c6". The
     revision is "unknown" because Minino does not embed the git metadata;
     the viewer accepts that and only uses it to warn about an older
     firmware. */
  build_date_iso(date, sizeof(date));
  snprintf(text, sizeof(text),
           "VERSION {\"revision\":\"unknown\",\"build_date\":\"%s\","
           "\"build_timestamp\":\"%sT00:00:00Z\",\"profile\":\"esp32c6\"}\n",
           date, date);
  return emit(out, len, text);
}

size_t esdr_reply_limits(char* out, size_t len) {
  /* The viewer validates the shape of this JSON: gain is [min, max, step]
     with the step dividing the range, bandwidth is null or
     [min, max, step, default] in MHz, rates is a list of sample rates it
     knows and bits a list of 8 and 10.

     Minino does not implement the manual gain, so the gain range is a single
     point, and it does not implement the analog filter, so the bandwidth is
     null. */
  return emit(out, len,
              "LIMITS {\"gain\":[0,0,1],\"bandwidth\":null,"
              "\"rates\":[80000000,40000000,16000000],\"bits\":[8,10]}\n");
}

size_t esdr_reply_range(char* out, size_t len) {
  char text[64];
  snprintf(text, sizeof(text), "RANGE %" PRIu32 " %" PRIu32 " 1\n", (uint32_t)ESDR_FREQ_MIN_MHZ,
           (uint32_t)ESDR_FREQ_MAX_MHZ);
  return emit(out, len, text);
}

size_t esdr_reply_transport(char* out, size_t len) {
  /* The viewer insists on "TRANSPORT USB 0": the number is the baud rate,
     and a USB link has none. */
  return emit(out, len, "TRANSPORT USB 0\n");
}

size_t esdr_reply_sync(char* out, size_t len, uint64_t nonce) {
  char text[48];
  snprintf(text, sizeof(text), "SYNC %" PRIu64 "\n", nonce);
  return emit(out, len, text);
}

size_t esdr_reply_gain(char* out, size_t len, bool manual, uint32_t index) {
  char text[64];
  snprintf(text, sizeof(text), "GAIN %s %" PRIu32 " 0 0 0\n", manual ? "MANUAL" : "HARDWARE",
           (uint32_t)index);
  return emit(out, len, text);
}

size_t esdr_reply_data(char* out, size_t len, uint32_t samples, uint32_t crc, uint32_t capture_us) {
  char text[64];
  snprintf(text, sizeof(text), "DATA %" PRIu32 " %08" PRIx32 " %" PRIu32 "\n", (uint32_t)samples, crc,
           (uint32_t)capture_us);
  return emit(out, len, text);
}
