/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Espectros calculados en el chip: el modo "On-Chip Spectrum" del visor de
 * ESPARGOS. Ver esdr_spectrum.h.
 */

#include "esdr_spectrum.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esdr_burst_proto.h"
#include "esdr_transport.h"

/* --------------------------------------------------------------------------
 * El protocolo
 *
 * Todo lo que se manda al host son lineas de texto y thenas de bytes con un
 * CRC32 al final. El formato es el del firmware del ESPARGOS y el visor lo
 * comprueba con expresiones regulares, asi que no hay margen.
 * -------------------------------------------------------------------------- */

#define SPEC_HEADER_BYTES 28u
#define SPEC_MAGIC        "SPC1"

/* Las tasas que el C6 sabe hacer, con su codigo en el protocolo. */
static const uint32_t s_rates[] = {80000000u, 40000000u, 0u,       0u,
                                   0u,        0u,        16000000u};
#define SPEC_RATE_COUNT (sizeof(s_rates) / sizeof(s_rates[0]))

/* El modo de correccion de la continua: 0 la quita en cada FFT, 1 la sigue
   despacio. El 0 es el que no toca nada de forma observable. */
static unsigned s_dc_mode = 1;

/* --------------------------------------------------------------------------
 * El codificador de potencia
 *
 * Un codigo representa 20*log10(potencia) en el dominio Q15 normalizado,
 * acotado a 0..255. El visor lo convierte a dBFS con la normalizacion de la
 * ventana de Hann.
 * -------------------------------------------------------------------------- */

static inline uint32_t complex_power(int16_t re, int16_t im) {
  return (uint32_t) ((int32_t) re * re) + (uint32_t) ((int32_t) im * im);
}

static inline uint8_t log_power_code(unsigned exponent, uint32_t norm) {
  static const uint16_t log2_q12[17] = {0,    358,  696,  1016, 1319, 1607,
                                        1882, 2144, 2396, 2637, 2869, 3092,
                                        3307, 3515, 3715, 3908, 4096};
  unsigned slot = (norm - 256u) >> 4, frac = norm & 15u;
  uint32_t logarithm = (exponent << 12) + log2_q12[slot] +
                       ((log2_q12[slot + 1] - log2_q12[slot]) * frac + 8) / 16;
  unsigned code =
      (logarithm * 1541u + (1u << 19)) >> 20; /* 20 log10(potencia) */
  return code > 255 ? 255 : (uint8_t) code;
}

static inline uint8_t power_code(uint32_t power) {
  if (!power) {
    return 0;
  }
  unsigned exponent = 31u - (unsigned) __builtin_clz(power);
  return log_power_code(exponent, (power << (31u - exponent)) >> 23);
}

/* Las potencias promediadas conservan decimales: truncarlas a entero
   hundiria las bins quietas varios pasos de cable. */
static inline uint8_t mean_power_code(float power) {
  union {
    float f;
    uint32_t u;
  } value = {.f = power};
  if (value.u < 0x3f800000u || value.u > 0x7f800000u) {
    return 0;
  }
  unsigned exponent = (value.u >> 23) - 127u;
  if (exponent >= 43) {
    return 255;
  }
  return log_power_code(exponent, 256u + ((value.u & 0x7fffffu) >> 15));
}

/* --------------------------------------------------------------------------
 * La FFT
 *
 * Radix-2 en tiempo decreciente, entrada en orden natural y salida en orden
 * natural. Los datos crudos traen I en los bits 9-0 y Q en los 19-10, con
 * signo de diez bits.
 * -------------------------------------------------------------------------- */

/* La tabla de raices: exp(-2*pi*i*k/n) en Q15, con n el tamano de la FFT.
   En la etapa de tamano 2*half se usa el indice k = group * (n / (2*half)). */
typedef struct {
  int16_t* re;
  int16_t* im;
  unsigned n;
} fft_twiddles_t;

static void twiddles_free(fft_twiddles_t* tw) {
  free(tw->re);
  free(tw->im);
  tw->re = NULL;
  tw->im = NULL;
  tw->n = 0;
}

static bool twiddles_build(fft_twiddles_t* tw, unsigned n) {
  tw->re = (int16_t*) malloc(sizeof(int16_t) * (n / 2u));
  tw->im = (int16_t*) malloc(sizeof(int16_t) * (n / 2u));
  if (tw->re == NULL || tw->im == NULL) {
    twiddles_free(tw);
    return false;
  }
  for (unsigned k = 0; k < n / 2u; k++) {
    double angle = -2.0 * M_PI * (double) k / (double) n;
    tw->re[k] = (int16_t) lrint(32767.0 * cos(angle));
    tw->im[k] = (int16_t) lrint(32767.0 * sin(angle));
  }
  tw->n = n;
  return true;
}

static inline int16_t sat16(int32_t v) {
  return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t) v;
}

/* FFT in situ sobre pares intercalados re/im. */
static void fft_inplace(int16_t* x, unsigned n, const fft_twiddles_t* tw) {
  /* La permutacion de inversion de bits. */
  for (unsigned i = 1, j = 0; i < n; i++) {
    unsigned bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      int16_t tr = x[2 * i], ti = x[2 * i + 1];
      x[2 * i] = x[2 * j];
      x[2 * i + 1] = x[2 * j + 1];
      x[2 * j] = tr;
      x[2 * j + 1] = ti;
    }
  }

  /* Las mariposas: en la etapa de tamano len hay len/2 mariposas por bloque
     de len muestras, y la j usa la raiz W_len^j. La tabla guarda
     exp(-2*pi*i*k/n), asi que el indice es j * (n/len).

     Cada mariposa divide entre dos, de modo que la ganancia total de la
     transformada es 1/n: sin eso, un tono fuerte saturaria el Q15 a la
     primera etapa. */
  for (unsigned len = 2; len <= n; len <<= 1) {
    unsigned half = len >> 1;
    unsigned step = n / len;
    for (unsigned block = 0; block < n; block += len) {
      for (unsigned j = 0; j < half; j++) {
        int32_t wr = tw->re[j * step];
        int32_t wi = tw->im[j * step];
        unsigned a = block + j;
        unsigned b = a + half;
        int32_t ar = x[2 * a], ai = x[2 * a + 1];
        int32_t br = x[2 * b], bi = x[2 * b + 1];
        uint32_t tr = (uint32_t) (wr * br) + (uint32_t) (wi * bi);
        uint32_t ti = (uint32_t) (wr * bi) - (uint32_t) (wi * br);
        uint32_t rr = (uint32_t) (ar * 32767) + 32767u;
        uint32_t ri = (uint32_t) (ai * 32767) + 32767u;
        x[2 * a] = (int16_t) ((rr + tr) >> 16);
        x[2 * a + 1] = (int16_t) ((ri + ti) >> 16);
        x[2 * b] = (int16_t) ((rr - tr) >> 16);
        x[2 * b + 1] = (int16_t) ((ri - ti) >> 16);
      }
    }
  }
}

/* --------------------------------------------------------------------------
 * El espacio de trabajo
 *
 * Va del heap: la imagen DRAM esta al limite del banco de captura y no cabe
 * ni un bin mas en ella.
 * -------------------------------------------------------------------------- */

typedef struct {
  int16_t* fft;    /* 2*n: re e im intercalados */
  int16_t* window; /* n: ventana de Hann en Q15 */
  float* powers;   /* n: potencias acumuladas */
  uint8_t* frame;  /* cabecera + bins + CRC */
  fft_twiddles_t tw;
  unsigned n;
} spectrum_workspace_t;

static void workspace_free(spectrum_workspace_t* ws) {
  free(ws->fft);
  free(ws->window);
  free(ws->powers);
  free(ws->frame);
  twiddles_free(&ws->tw);
  memset(ws, 0, sizeof(*ws));
}

static bool workspace_alloc(spectrum_workspace_t* ws, unsigned n) {
  memset(ws, 0, sizeof(*ws));
  ws->fft = (int16_t*) malloc(sizeof(int16_t) * 2u * n);
  ws->window = (int16_t*) malloc(sizeof(int16_t) * n);
  ws->powers = (float*) malloc(sizeof(float) * n);
  ws->frame = (uint8_t*) malloc(SPEC_HEADER_BYTES + n + 4u);
  if (ws->fft == NULL || ws->window == NULL || ws->powers == NULL ||
      ws->frame == NULL) {
    workspace_free(ws);
    return false;
  }
  for (unsigned j = 0; j < n; j++) {
    ws->window[j] = (int16_t) lrint(
        16383.5 * (1.0 - cos(2.0 * M_PI * (double) j / (double) n)));
  }
  if (!twiddles_build(&ws->tw, n)) {
    workspace_free(ws);
    return false;
  }
  ws->n = n;
  return true;
}

/* --------------------------------------------------------------------------
 * La correccion de la continua
 *
 * La ventana de Hann tiene el bin de continua y sus dos vecinos mas altos
 * que el resto, y la continua del conversor se colaria en los tres. Se
 * estima y se resta.
 * -------------------------------------------------------------------------- */

typedef struct {
  int32_t re, im;
  bool ready;
} dc_tracker_t;

static void dc_apply(dc_tracker_t* dc, int16_t* x, unsigned n) {
  int32_t dr = x[0], di = x[1];

  if (s_dc_mode) {
    if (!dc->ready) {
      dc->re = dr * 256;
      dc->im = di * 256;
      dc->ready = true;
    } else {
      dc->re += (dr * 256 - dc->re) >> 6;
      dc->im += (di * 256 - dc->im) >> 6;
    }
    dr = (dc->re + 128) >> 8;
    di = (dc->im + 128) >> 8;
  }

  x[0] = sat16(x[0] - dr);
  x[1] = sat16(x[1] - di);
  for (unsigned k = 0; k < 2; k++) {
    unsigned j = k ? n - 1 : 1; /* los dos vecinos de la continua */
    x[2 * j] = sat16(x[2 * j] + dr / 2);
    x[2 * j + 1] = sat16(x[2 * j + 1] + di / 2);
  }
}

/* --------------------------------------------------------------------------
 * El marco
 * -------------------------------------------------------------------------- */

static inline void put16(uint8_t* at, uint16_t v) {
  at[0] = (uint8_t) v;
  at[1] = (uint8_t) (v >> 8);
}

static inline void put32(uint8_t* at, uint32_t v) {
  put16(at, (uint16_t) v);
  put16(at + 2, (uint16_t) (v >> 16));
}

static inline void put64(uint8_t* at, uint64_t v) {
  put32(at, (uint32_t) v);
  put32(at + 4, (uint32_t) (v >> 32));
}

/**
 * @brief Empaqueta el lote acumulado en un marco y lo deja listo para enviar.
 *
 * @return los bytes del marco.
 */
static size_t pack_spectrum(spectrum_workspace_t* ws,
                            unsigned n,
                            unsigned log2n,
                            unsigned detector,
                            unsigned merged,
                            uint32_t frames,
                            uint64_t index,
                            uint8_t gain) {
  uint8_t* frame = ws->frame;

  memcpy(frame, SPEC_MAGIC, 4);
  put32(frame + 4, frames);
  put64(frame + 8, index);
  put32(frame + 16, n * merged);
  put16(frame + 20, (uint16_t) merged);
  frame[22] = 8 | (detector ? 1 : 0); /* bit 3: hay huecos entre capturas */
  frame[23] = gain;
  put16(frame + 24, 0);
  frame[26] = (uint8_t) log2n;
  frame[27] = 2; /* multiplicador del codigo en dB */

  float mean_scale = 1.0f / (float) merged;
  for (unsigned j = 0; j < n; j++) {
    frame[SPEC_HEADER_BYTES + j] =
        detector ? power_code((uint32_t) ws->powers[j])
                 : mean_power_code(ws->powers[j] * mean_scale);
  }
  put32(frame + SPEC_HEADER_BYTES + n,
        esdr_crc32(frame, SPEC_HEADER_BYTES + n));
  memset(ws->powers, 0, n * sizeof(float));
  return SPEC_HEADER_BYTES + n + 4u;
}

/* --------------------------------------------------------------------------
 * El comando
 * -------------------------------------------------------------------------- */

/* Lo que el visor necesita para saber que perfiles hay. */
static void send_capabilities(void) {
  char text[192];
  int written = snprintf(
      text, sizeof(text),
      "SPECINFO {\"continuous\":false,\"transports\":[\"USB\"],\"profiles\":[");
  if (written < 0) {
    return;
  }
  esdr_host_write(text, (size_t) written);

  bool first = true;
  for (unsigned r = 0; r < SPEC_RATE_COUNT; r++) {
    if (!s_rates[r]) {
      continue;
    }
    for (unsigned n = 256; n <= ESDR_SPEC_MAX_FFT; n *= 2) {
      written =
          snprintf(text, sizeof(text), "%s[%" PRIu32 ",%u,%" PRIu32 ",1,1,0]",
                   first ? "" : ",", s_rates[r], r, (uint32_t) n);
      if (written < 0) {
        return;
      }
      esdr_host_write(text, (size_t) written);
      first = false;
    }
  }
  esdr_host_write("]}\n", 3);
}

bool esdr_spectrum_command(const char* line,
                           uint32_t freq_mhz,
                           esdr_spectrum_acquire_fn acquire) {
  unsigned dc_mode;
  char dc_extra;

  if (!strcmp(line, "DC?")) {
    char text[16];
    snprintf(text, sizeof(text), "DC %u\n", s_dc_mode);
    esdr_host_write(text, strlen(text));
    return true;
  }
  if (sscanf(line, "DC %u %c", &dc_mode, &dc_extra) == 1 && dc_mode < 2) {
    s_dc_mode = dc_mode;
    esdr_host_write("OK\n", 3);
    return true;
  }
  if (!strcmp(line, "SPECINFO?")) {
    send_capabilities();
    return true;
  }
  if (strncmp(line, "SPEC ", 5)) {
    return false;
  }

  unsigned ms = 0, stride = 0, units = 0, detector = 0, rate = 0, n = 0,
           stats = 0;
  char extra = 0;
  int fields = sscanf(line, "SPEC %u %u %u %u %u %u %u %c", &ms, &stride,
                      &units, &detector, &rate, &n, &stats, &extra);
  if ((fields != 6 && fields != 7) || stats > 1 || ms > 86400000u ||
      stride != 1 || !units || units > 8 || detector > 1 ||
      rate >= SPEC_RATE_COUNT || !s_rates[rate] || n < 256 ||
      n > ESDR_SPEC_MAX_FFT || (n & (n - 1))) {
    esdr_host_write("ERR spec_args\n", 14);
    return true;
  }

  spectrum_workspace_t ws;
  if (!workspace_alloc(&ws, n)) {
    esdr_host_write("ERR spectrum_memory\n", 20);
    return true;
  }

  unsigned log2n = 0;
  while ((1u << log2n) < n) {
    log2n++;
  }

  char text[192];
  int written =
      snprintf(text, sizeof(text),
               "SPEC %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
               (uint32_t) n, s_rates[rate], (uint32_t) n, (uint32_t) freq_mhz);
  esdr_host_write(text, (size_t) written);

  uint32_t frames = 0, ffts = 0, status = 0;
  int64_t yield_at = esdr_host_elapsed_ms();
  uint64_t pairs = 0;
  uint64_t index = 0;
  unsigned merged = 0;
  uint8_t gain = 0;
  bool stopped = false;
  dc_tracker_t dc = {0};

  while (!ms || 1) {
    if (esdr_host_stop_requested()) {
      stopped = true;
      break;
    }
    if (ms && esdr_host_elapsed_ms() > ms) {
      break;
    }

    if (!merged) {
      index = (uint64_t) esdr_host_elapsed_ms() * s_rates[rate] / 1000u;
    }

    const uint32_t* words = NULL;
    uint32_t elapsed = 0;
    if (!acquire(n, rate, &words, &elapsed)) {
      status = 1;
      break;
    }
    if (!merged) {
      gain = (uint8_t) (words[0] >> 20);
    }

    for (unsigned j = 0; j < n; j++) {
      int32_t i = ((int32_t) (words[j] << 22) >> 22);
      int32_t q = ((int32_t) (words[j] << 12) >> 22);
      ws.fft[2 * j] = sat16((i * ws.window[j]) >> 9);
      ws.fft[2 * j + 1] = sat16((q * ws.window[j]) >> 9);
    }
    fft_inplace(ws.fft, n, &ws.tw);
    dc_apply(&dc, ws.fft, n);

    for (unsigned j = 0; j < n; j++) {
      uint32_t p = complex_power(ws.fft[2 * j], ws.fft[2 * j + 1]);
      if (detector) {
        if (p > (uint32_t) ws.powers[j]) {
          ws.powers[j] = (float) p;
        }
      } else {
        ws.powers[j] += (float) p;
      }
    }
    ffts++;
    pairs += n;
    merged++;

    if (merged >= units) {
      size_t size =
          pack_spectrum(&ws, n, log2n, detector, merged, frames, index, gain);
      esdr_host_write(ws.frame, size);
      frames++;
      merged = 0;
    }

    /* El bucle no puede quedarse con la CPU: la tarea idle es la que
       alimenta el watchdog, asi que hay que cederle el paso de vez en
       cuando (el firmware del ESPARGOS hace lo mismo). */
    if (esdr_host_elapsed_ms() - yield_at >= 20) {
      esdr_host_yield();
      yield_at = esdr_host_elapsed_ms();
    }
  }

  /* Lo que quedo sin empaquetar se manda igual, para que el ultimo cuadro
     no se pierda. */
  if (!status && merged) {
    size_t size =
        pack_spectrum(&ws, n, log2n, detector, merged, frames, index, gain);
    esdr_host_write(ws.frame, size);
    frames++;
  }

  written = snprintf(
      text, sizeof(text),
      "SPECEND %u 0 %u %" PRIu64 " %" PRIu64 " 0 0 %u 0 0 %u %u\n",
      (unsigned) status, (unsigned) ffts, pairs, esdr_host_elapsed_ms(),
      (unsigned) frames, (unsigned) ffts, (unsigned) stopped);
  esdr_host_write(text, (size_t) written);

  workspace_free(&ws);
  return true;
}
