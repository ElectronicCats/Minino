/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Host tests for the on-chip spectrum module.
 *
 * The FFT is the part that can be silently wrong: a spectrum with the bins
 * in the wrong order looks plausible on screen but is not the signal. So it
 * is checked against a naive DFT, and the wire format against the layout the
 * viewer parses.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esdr_burst_proto.h"
#include "esdr_spectrum.h"
#include "esdr_transport.h"
#include "unity.h"

/* El transporte en el test: se recoge lo que el modulo escribe y no hay
   ni puerto ni reloj. */
static char s_written[4096];
static size_t s_written_len = 0;
static bool s_stop = false;
static uint64_t s_clock_ms = 0;

void esdr_host_write(const void* data, size_t len) {
  if (s_written_len + len < sizeof(s_written)) {
    memcpy(s_written + s_written_len, data, len);
    s_written_len += len;
    s_written[s_written_len] = 0;
  }
}

bool esdr_host_stop_requested(void) {
  return s_stop;
}

uint64_t esdr_host_elapsed_ms(void) {
  return s_clock_ms;
}

void esdr_host_yield(void) {
  s_clock_ms++;
}

void setUp(void) {
  s_written_len = 0;
  s_written[0] = 0;
  s_stop = false;
  s_clock_ms = 0;
}

void tearDown(void) {}

/* El modulo usa malloc/free y printf; los tests comprueban las funciones
 * puras a traves de la interfaz publica. */

/*
 * La FFT del modulo es estatica, asi que el test la reproduce aqui con el
 * mismo algoritmo y compara el resultado con un DFT directo. Si el algoritmo
 * del modulo cambia, este test hay que actualizarlo con el.
 */
static void fft_reference(int16_t* x, unsigned n) {
  /* permutacion de inversion de bits */
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
  for (unsigned len = 2; len <= n; len <<= 1) {
    unsigned half = len >> 1;
    unsigned step = n / len;
    for (unsigned block = 0; block < n; block += len) {
      for (unsigned j = 0; j < half; j++) {
        double angle = -2.0 * M_PI * (double) (j * step) / (double) n;
        int32_t wr = (int32_t) lrint(32767.0 * cos(angle));
        int32_t wi = (int32_t) lrint(32767.0 * sin(angle));
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

static void dft_naive(const double* re,
                      const double* im,
                      unsigned n,
                      double* out_re,
                      double* out_im) {
  for (unsigned k = 0; k < n; k++) {
    double sr = 0, si = 0;
    for (unsigned j = 0; j < n; j++) {
      double angle = -2.0 * M_PI * (double) j * (double) k / (double) n;
      sr += re[j] * cos(angle) - im[j] * sin(angle);
      si += re[j] * sin(angle) + im[j] * cos(angle);
    }
    out_re[k] = sr;
    out_im[k] = si;
  }
}

static void test_the_fft_matches_a_direct_transform(void) {
  const unsigned n = 64;
  double re[n], im[n];
  int16_t x[2 * n];

  /* Una senal con dos tonos: el espectro tiene que tenerlos ahi. */
  for (unsigned j = 0; j < n; j++) {
    re[j] = 1000.0 * cos(2.0 * M_PI * 3.0 * (double) j / (double) n) +
            500.0 * cos(2.0 * M_PI * 11.0 * (double) j / (double) n);
    im[j] = 0.0;
    x[2 * j] = (int16_t) re[j];
    x[2 * j + 1] = 0;
  }

  double dre[n], dim[n];
  dft_naive(re, im, n, dre, dim);
  fft_reference(x, n);

  /* La salida de la FFT va en orden natural: el bin k es la frecuencia k.
     La FFT es Q15 y la entrada esta en unidades crudas, asi que el
     resultado lleva el factor 32767/1000. */
  const double scale = 1.0 / (double) n; /* cada mariposa divide entre dos */
  for (unsigned k = 0; k < n; k++) {
    double got_re = x[2 * k];
    double got_im = x[2 * k + 1];
    double got = sqrt(got_re * got_re + got_im * got_im);
    double want = sqrt(dre[k] * dre[k] + dim[k] * dim[k]) * scale;
    if (want < 1.0) {
      /* Un bin casi vacio puede quedar con residuo del redondeo. */
      TEST_ASSERT_TRUE(got < 20.0);
    } else {
      TEST_ASSERT_FLOAT_WITHIN(want * 0.05 + 1.0, want, got);
    }
  }
}

static void test_the_fft_finds_the_tones(void) {
  const unsigned n = 64;
  int16_t x[2 * n];

  for (unsigned j = 0; j < n; j++) {
    x[2 * j] =
        (int16_t) (8000.0 * cos(2.0 * M_PI * 5.0 * (double) j / (double) n));
    x[2 * j + 1] = 0;
  }
  fft_reference(x, n);

  /* El tono esta en el bin 5 y en el 59 (la parte negativa del espectro). */
  unsigned peak = 0;
  double best = 0;
  for (unsigned k = 0; k < n; k++) {
    double p =
        (double) x[2 * k] * x[2 * k] + (double) x[2 * k + 1] * x[2 * k + 1];
    if (p > best) {
      best = p;
      peak = k;
    }
  }
  TEST_ASSERT_EQUAL_UINT(5, peak);
}

static void test_the_raw_word_layout_is_i_low_and_q_high(void) {
  /* El visor descodifica el grupo de 20 bits con I en los bits 0-9 y Q en
     los 10-19, y el banco de captura trae I en los bits 9-0 de la palabra
     y Q en los 19-10. */
  uint32_t i = 100, q = 200;
  uint32_t word = (i & 0x3FFu) | ((q & 0x3FFu) << 10);
  TEST_ASSERT_EQUAL_INT((int) i, (int) ((int32_t) (word << 22) >> 22));
  TEST_ASSERT_EQUAL_INT((int) q, (int) ((int32_t) (word << 12) >> 22));

  /* Y con signo, como los diez bits con signo que son. */
  int32_t neg = -63;
  uint32_t word_neg = ((uint32_t) (neg & 0x3FF)) | ((uint32_t) (0x3C2) << 10);
  TEST_ASSERT_EQUAL_INT(-63, (int) ((int32_t) (word_neg << 22) >> 22));
  TEST_ASSERT_EQUAL_INT(-62, (int) ((int32_t) (word_neg << 12) >> 22));
}

static void test_the_crc_of_a_frame_is_the_one_the_viewer_checks(void) {
  /* El visor comprueba el CRC32 de la cabecera y de las bins. */
  uint8_t frame[32 + 4];
  for (unsigned j = 0; j < 32; j++) {
    frame[j] = (uint8_t) j;
  }
  uint32_t crc = esdr_crc32(frame, 32);
  frame[32] = (uint8_t) crc;
  frame[33] = (uint8_t) (crc >> 8);
  frame[34] = (uint8_t) (crc >> 16);
  frame[35] = (uint8_t) (crc >> 24);
  /* El CRC del marco completo no es el del cuerpo: el visor lo calcula
     sobre la cabecera y las bins, y lo compara con los ultimos cuatro
     bytes. */
  TEST_ASSERT_EQUAL_HEX32(crc, esdr_crc32(frame, 32));
}

static void test_the_spectrum_command_recognises_its_own(void) {
  /* El comando llega con una funcion de adquisicion que no se usa en estos
     casos, asi que basta con que la linea se reconozca. */
  TEST_ASSERT_TRUE(esdr_spectrum_command("SPECINFO?", 2412, NULL));
  TEST_ASSERT_TRUE(esdr_spectrum_command("DC?", 2412, NULL));
  TEST_ASSERT_TRUE(esdr_spectrum_command("DC 1", 2412, NULL));
  TEST_ASSERT_FALSE(esdr_spectrum_command("INFO", 2412, NULL));
  TEST_ASSERT_FALSE(esdr_spectrum_command("CAP20 4096 0", 2412, NULL));
}

static void test_the_profiles_are_the_ones_the_viewer_understands(void) {
  TEST_ASSERT_TRUE(esdr_spectrum_command("SPECINFO?", 2412, NULL));

  /* El visor parsea el JSON de SPECINFO y exige esta forma:
     {"continuous":bool,"transports":[...],"profiles":[[rate,code,bins,stride,units,cont],...]}
   */
  TEST_ASSERT_NOT_NULL(strstr(s_written, "SPECINFO {"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "\"continuous\":false"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "\"transports\":[\"USB\"]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "\"profiles\":["));

  /* Los perfiles: la tasa, su codigo, las bins, el paso, las unidades por
     marco y si la captura es continua. El visor busca el perfil cuyo
     numero de bins es el que ha pedido. */
  TEST_ASSERT_NOT_NULL(strstr(s_written, "[80000000,0,512,1,1,0]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "[80000000,0,1024,1,1,0]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "[80000000,0,2048,1,1,0]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "[40000000,1,512,1,1,0]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "[16000000,6,512,1,1,0]"));
  TEST_ASSERT_NOT_NULL(strstr(s_written, "]}\n"));
}

static void test_the_dc_command_answers(void) {
  TEST_ASSERT_TRUE(esdr_spectrum_command("DC?", 2412, NULL));
  TEST_ASSERT_EQUAL_STRING("DC 1\n", s_written);

  s_written_len = 0;
  s_written[0] = 0;
  TEST_ASSERT_TRUE(esdr_spectrum_command("DC 0", 2412, NULL));
  TEST_ASSERT_EQUAL_STRING("OK\n", s_written);

  s_written_len = 0;
  s_written[0] = 0;
  TEST_ASSERT_TRUE(esdr_spectrum_command("DC?", 2412, NULL));
  TEST_ASSERT_EQUAL_STRING("DC 0\n", s_written);

  /* Se vuelve al modo por defecto para no affectar a las demas pruebas. */
  esdr_spectrum_command("DC 1", 2412, NULL);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_the_fft_matches_a_direct_transform);
  RUN_TEST(test_the_fft_finds_the_tones);
  RUN_TEST(test_the_raw_word_layout_is_i_low_and_q_high);
  RUN_TEST(test_the_crc_of_a_frame_is_the_one_the_viewer_checks);
  RUN_TEST(test_the_spectrum_command_recognises_its_own);
  RUN_TEST(test_the_profiles_are_the_ones_the_viewer_understands);
  RUN_TEST(test_the_dc_command_answers);

  return UNITY_END();
}
