/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The burst protocol that the ESP-SDR firmware speaks, and that the
 * ESP-WebSDR browser viewer drives (https://espargos.net/espsdr/app/).
 *
 * Ported from ESPARGOS/esp-sdr (GPL-3.0-or-later): main/common/burst_commands.h,
 * main/common/burst_capture.h, main/common/burst_limits.h and docs/protocol.md.
 * The wire format is fixed by the viewer, so this is a port and not a
 * reimplementation: the acceptance rules of the parser and the exact bytes of
 * the replies have to match what the browser expects.
 *
 * The module is plain C with no ESP-IDF dependency, so the host tests in
 * test/ can exercise it without hardware.
 */

#ifndef ESDR_BURST_PROTO_H_
#define ESDR_BURST_PROTO_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Longest command line the firmware accepts, and the longest reply it
   produces. The viewer reads a reply with a 256 byte budget and the browser
   refuses anything longer, so every reply has to stay well below that. */
#define ESDR_LINE_MAX 128
#define ESDR_REPLY_MAX 256

/* A burst is at most this long: the dump engine writes 16380 words and the
   viewer refuses an identity that promises more than 16384. */
#define ESDR_MAX_SAMPLES 16380
#define ESDR_MIN_SAMPLES 256

/* The C6 dump engine takes a clock divider in bits 15-16 of the control
   register: rate codes 0, 1 and 6 select 80, 40 and 16 MS/s. */
#define ESDR_RATE_80MSPS 0u
#define ESDR_RATE_40MSPS 1u
#define ESDR_RATE_16MSPS 6u

/* The identity the viewer expects. The regex it applies is
   /^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$/ and the burst
   count has to be between 4096 and 16384. */
#define ESDR_FAMILY_ID "C6SDR"
#define ESDR_PROTOCOL_VERSION 6

/* The lowest and highest frequency Minino can actually tune: the dump engine
   borrows the Wi-Fi PHY, so only the Wi-Fi channels are reachable. */
#define ESDR_FREQ_MIN_MHZ 2412u
#define ESDR_FREQ_MAX_MHZ 2472u

/* Sample formats the viewer can ask for. The name is the number of bits per
   I and per Q component. */
typedef enum {
  ESDR_FORMAT_IQ8 = 16,  /* CAP16: signed 8 bit I then Q */
  ESDR_FORMAT_IQ10 = 20, /* CAP20: packed signed 10 bit I/Q, 20 bits per pair */
} esdr_format_t;

typedef enum {
  ESDR_CMD_UNKNOWN = 0,
  ESDR_CMD_SYNC,
  ESDR_CMD_INFO,
  ESDR_CMD_CAPS,
  ESDR_CMD_VERSION,
  ESDR_CMD_TRANSPORT,
  ESDR_CMD_RELEASE,
  ESDR_CMD_FREQ,
  ESDR_CMD_RANGE,
  ESDR_CMD_LIMITS,
  ESDR_CMD_GAIN,
  ESDR_CMD_BANDWIDTH,
  ESDR_CMD_GPIO,
  ESDR_CMD_CAP16,
  ESDR_CMD_CAP20,
  ESDR_CMD_RXRUN,
  ESDR_CMD_MININO,
  ESDR_CMD_SPEC,
  ESDR_CMD_BAUD,
} esdr_cmd_t;

typedef struct {
  esdr_cmd_t cmd;
  /* La linea entera, para los comandos que se pasan tal cual (SPEC). */
  char args[ESDR_LINE_MAX];
  uint64_t nonce;      /* SYNC */
  uint32_t freq_mhz;   /* FREQ */
  uint32_t samples;    /* CAP16, CAP20, RXRUN */
  uint32_t rate;       /* CAP16, CAP20, RXRUN: the rate code */
  uint32_t repeats;    /* RXRUN */
  uint32_t format;     /* RXRUN: 16 or 20 */
  uint32_t gain_index; /* GAIN MANUAL */
  bool gain_manual;    /* GAIN */
} esdr_request_t;

/**
 * @brief Parse one command line.
 *
 * The acceptance rules are the ones of the upstream firmware: a command with
 * trailing garbage is rejected, an empty line is not a command at all.
 *
 * @param line NUL terminated line, without the line terminator
 * @param out Filled with the decoded command
 * @return true when the line is a command the firmware answers
 */
bool esdr_parse_line(const char* line, esdr_request_t* out);

/**
 * @brief Pack the raw dump words as signed 8 bit I then Q, in place.
 *
 * The dump word carries I in bits 9-0 and Q in bits 19-10; the upper eight
 * bits of each ten bit field are what the viewer expects.
 *
 * @return The number of bytes written, 2 * count.
 */
size_t esdr_pack_iq8(uint32_t* words, size_t count);

/**
 * @brief Pack the raw dump words as signed 10 bit I/Q, in place.
 *
 * Twenty bits per pair, least significant bit first: I in bits 0-9 and Q in
 * bits 10-19 of each group, which is the order the viewer decodes.
 *
 * @return The number of bytes written, (count * 20 + 7) / 8.
 */
size_t esdr_pack_iq10(uint32_t* words, size_t count);

/**
 * @brief Bytes a capture of @p count samples takes on the wire.
 */
size_t esdr_wire_size(size_t count, esdr_format_t format);

/**
 * @brief CRC32 of a payload, the one the viewer checks the capture with.
 *
 * Same polynomial and conventions as zlib's crc32, so the browser's own
 * implementation agrees byte for byte.
 */
uint32_t esdr_crc32(const uint8_t* data, size_t len);

/**
 * @brief The reply lines, already terminated with a newline.
 *
 * Each one returns the number of bytes written, 0 when the buffer was too
 * small.
 */
size_t esdr_reply_info(char* out, size_t len);
size_t esdr_reply_caps(char* out, size_t len);
size_t esdr_reply_version(char* out, size_t len);
size_t esdr_reply_limits(char* out, size_t len);
size_t esdr_reply_range(char* out, size_t len);
size_t esdr_reply_transport(char* out, size_t len);
size_t esdr_reply_sync(char* out, size_t len, uint64_t nonce);
size_t esdr_reply_gain(char* out, size_t len, bool manual, uint32_t index);
size_t esdr_reply_data(char* out, size_t len, uint32_t samples, uint32_t crc, uint32_t capture_us);

#ifdef __cplusplus
}
#endif

#endif /* ESDR_BURST_PROTO_H_ */
