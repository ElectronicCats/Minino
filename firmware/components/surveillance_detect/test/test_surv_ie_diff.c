// SPDX-License-Identifier: GPL-3.0-or-later
//
// Test diferencial del fingerprint de IE.
//
// Ejecuta nuestro comparador binario (surv_ie.c) y el constructor de string
// original de flock-you (test/reference/) sobre las mismas entradas, y exige
// que coincidan en TODAS. Es la unica garantia real de que la reimplementacion
// binaria conserva la semantica del algoritmo validado en campo: los tests
// normales comparten con el codigo cualquier malentendido que tengamos, este
// no.
//
// Historia: la primera version del comparador omitia los tres parches de
// tramas malformadas del original y fallaba 3020 de 25000 casos, siempre en la
// direccion de NO detectar camaras que la referencia si detecta. Ningun test
// convencional lo habria visto.
#include <string.h>
#include "surv_ie.h"
#include "surv_test.h"

bool surv_ref_is_primary(const uint8_t* body, int bodyLen);

static const uint8_t FLOCK_IES[] = {
    0x00, 0x00,              // SSID wildcard
    0x02, 0x02, 0xaa, 0xbb,  // tag 2
    0x0c, 0x01, 0x00,        // tag 12
    0x7f, 0x08, 0,    0,    0,    0,    0,    0,    0,    0x40,  // tag 127
    0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,  // vendor LiteON
    0x2d, 0x02, 0x00, 0x00,                                // tag 45
    0xbf, 0x02, 0x00, 0x00,                                // tag 191
    0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00,  // vendor WFA
};
static const uint8_t VEND_A[7] = {0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03};
static const uint8_t VEND_B[7] = {0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00};

static unsigned long s_rng;
static uint32_t rnd(void) {  // xorshift: deterministico, mismo resultado en CI
  s_rng ^= s_rng << 13;
  s_rng ^= s_rng >> 17;
  s_rng ^= s_rng << 5;
  return (uint32_t) s_rng;
}

static int s_disagree;
static int s_agree_true;
static int s_agree_false;
static char s_first[160];

static void check(const uint8_t* b, int n) {
  bool ref = surv_ref_is_primary(b, n);
  bool ours = surv_ie_matches_flock(b, n);
  if (ref != ours) {
    if (s_disagree == 0) {
      int p = snprintf(s_first, sizeof(s_first), "len=%d ref=%d nuestro=%d ", n,
                       (int) ref, (int) ours);
      for (int i = 0; i < n && p < (int) sizeof(s_first) - 3; i++) {
        p += snprintf(s_first + p, sizeof(s_first) - (size_t) p, "%02x", b[i]);
      }
    }
    s_disagree++;
  } else if (ref) {
    s_agree_true++;
  } else {
    s_agree_false++;
  }
}

static uint8_t s_buf[256];

static void run_corpus(void) {
  s_rng = 0x12345678u;
  s_disagree = 0;
  s_agree_true = 0;
  s_agree_false = 0;
  s_first[0] = '\0';

  check(FLOCK_IES, (int) sizeof(FLOCK_IES));

  // firma con cola de FCS
  memcpy(s_buf, FLOCK_IES, sizeof(FLOCK_IES));
  memset(s_buf + sizeof(FLOCK_IES), 0xAB, 4);
  check(s_buf, (int) sizeof(FLOCK_IES) + 4);

  // mutaciones de un bit
  for (size_t i = 0; i < sizeof(FLOCK_IES); i++) {
    for (int bit = 0; bit < 8; bit++) {
      memcpy(s_buf, FLOCK_IES, sizeof(FLOCK_IES));
      s_buf[i] ^= (uint8_t) (1u << bit);
      check(s_buf, (int) sizeof(FLOCK_IES));
    }
  }

  // mutaciones de dos bytes
  for (int t = 0; t < 60000; t++) {
    memcpy(s_buf, FLOCK_IES, sizeof(FLOCK_IES));
    s_buf[rnd() % sizeof(FLOCK_IES)] ^= (uint8_t) (1u << (rnd() % 8));
    s_buf[rnd() % sizeof(FLOCK_IES)] ^= (uint8_t) (1u << (rnd() % 8));
    check(s_buf, (int) sizeof(FLOCK_IES));
  }

  // buffers aleatorios
  for (int t = 0; t < 20000; t++) {
    int len = (int) (rnd() % 80) + 1;
    for (int i = 0; i < len; i++) {
      s_buf[i] = (uint8_t) rnd();
    }
    check(s_buf, len);
  }

  // prefijos aleatorios delante de la firma
  for (int t = 0; t < 5000; t++) {
    int pre = (int) (rnd() % 12);
    for (int i = 0; i < pre; i++) {
      s_buf[i] = (uint8_t) rnd();
    }
    memcpy(s_buf + pre, FLOCK_IES, sizeof(FLOCK_IES));
    check(s_buf, pre + (int) sizeof(FLOCK_IES));
  }

  // cadenas TLV validas y aleatorias: tocan muchos mas caminos que bytes
  // sueltos
  for (int t = 0; t < 120000; t++) {
    int n = 0;
    int nie = (int) (rnd() % 9) + 1;
    for (int k = 0; k < nie && n < 200; k++) {
      uint32_t r = rnd();
      uint8_t tag;
      switch (r % 10) {
        case 0:
          tag = 2;
          break;
        case 1:
          tag = 12;
          break;
        case 2:
          tag = 127;
          break;
        case 3:
          tag = 45;
          break;
        case 4:
          tag = 191;
          break;
        case 5:
          tag = 0;
          break;
        case 6:
        case 7:
          tag = 221;
          break;
        default:
          tag = (uint8_t) (rnd() % 256);
          break;
      }
      if (tag == 221) {
        int which = (int) ((rnd() >> 8) % 3);
        int el = (which == 2) ? (int) ((rnd() >> 16) % 10) : 7;
        if (n + 2 + el > 200)
          break;
        s_buf[n++] = 221;
        s_buf[n++] = (uint8_t) el;
        for (int q = 0; q < el; q++) {
          s_buf[n + q] = (which == 0)   ? (q < 7 ? VEND_A[q] : 0)
                         : (which == 1) ? (q < 7 ? VEND_B[q] : 0)
                                        : (uint8_t) rnd();
        }
        n += el;
      } else {
        int el = (int) ((rnd() >> 16) % 6);
        if (n + 2 + el > 200)
          break;
        s_buf[n++] = tag;
        s_buf[n++] = (uint8_t) el;
        for (int q = 0; q < el; q++) {
          s_buf[n + q] = (uint8_t) rnd();
        }
        n += el;
      }
    }
    if (n >= 2)
      check(s_buf, n);
  }

  // patrones de phantom overflow delante de la firma
  for (int t = 0; t < 3000; t++) {
    int n = 0;
    int nph = (int) (rnd() % 4);
    for (int k = 0; k < nph; k++) {
      s_buf[n++] = 64;
      s_buf[n++] = 128;
    }
    memcpy(s_buf + n, FLOCK_IES, sizeof(FLOCK_IES));
    n += (int) sizeof(FLOCK_IES);
    check(s_buf, n);
  }

  // cola canonica: ancla LiteON + 45 + 191 + vendor WFA. Es lo que la
  // referencia deja como "2,12,127," + esta cola tras canonizar, y por tanto
  // la unica parte que debe casar para detectar.
  static const uint8_t CANON_TAIL[] = {
      0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,
      0x2d, 0x02, 0x00, 0x00,                                // tag 45
      0xbf, 0x02, 0x00, 0x00,                                // tag 191
      0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00,  // vendor WFA
  };

  // cadenas de mas de 16 tokens con basura delante del ancla. Historia: el
  // primer cap de SURV_IE_MAX_TOKS (=16) hacia que build_tokens devolviera -1
  // al llenarse, y una trama con >=17 IEs y el ancla de LiteON atras NO se
  // detectaba, mientras la referencia (buffer de string de 128 caracteres)
  // canoniciza y SI la detecta. Estos casos son exactamente lo que el tope de
  // recorrido y el espejo del presupuesto deben cubrir; si se degradan, esto
  // falla.
  for (int t = 0; t < 20000; t++) {
    int n = 0;
    // 13..22 tokens de basura de 4 chars en string ("240," = 4): con la cola
    // de 36 chars queda bajo los 128 de la referencia.
    int njunk = (int) (rnd() % 10) + 13;
    for (int k = 0; k < njunk && n < 200; k++) {
      uint8_t tag =
          (uint8_t) (240 + (rnd() % 8));  // 240..247, jamas tags de sig
      int el = (int) ((rnd() >> 16) % 3);
      if (n + 2 + el > 200)
        break;
      s_buf[n++] = tag;
      s_buf[n++] = (uint8_t) el;
      for (int q = 0; q < el; q++) {
        s_buf[n + q] = (uint8_t) rnd();
      }
      n += el;
    }
    if (n + (int) sizeof(CANON_TAIL) > 200)
      continue;
    memcpy(s_buf + n, CANON_TAIL, sizeof(CANON_TAIL));
    n += (int) sizeof(CANON_TAIL);
    check(s_buf, n);
  }

  // la misma zona con tokens de 2-4 chars: la referencia alcanza runs de
  // tokens mas largos dentro de sus 128 caracteres, y el lado equivalente del
  // espejo de presupuesto (que ambos fallen o matcheen igual) se ejerce aqui.
  for (int t = 0; t < 20000; t++) {
    int n = 0;
    int njunk = (int) (rnd() % 24) + 24;
    for (int k = 0; k < njunk && n < 200; k++) {
      uint8_t tag = (uint8_t) ((rnd() % 247) + 1);  // 1..247, sin 0
      int el = (int) ((rnd() >> 16) % 2);
      if (n + 2 + el > 200)
        break;
      s_buf[n++] = tag;
      s_buf[n++] = (uint8_t) el;
      for (int q = 0; q < el; q++) {
        s_buf[n + q] = (uint8_t) rnd();
      }
      n += el;
    }
    if (n + (int) sizeof(CANON_TAIL) > 200)
      continue;
    memcpy(s_buf + n, CANON_TAIL, sizeof(CANON_TAIL));
    n += (int) sizeof(CANON_TAIL);
    check(s_buf, n);
  }

  // firma con IEs aleatorios detras
  for (int t = 0; t < 20000; t++) {
    memcpy(s_buf, FLOCK_IES, sizeof(FLOCK_IES));
    int n = (int) sizeof(FLOCK_IES);
    int post = (int) (rnd() % 3);
    for (int k = 0; k < post && n < 190; k++) {
      int el = (int) (rnd() % 5);
      s_buf[n++] = (uint8_t) rnd();
      s_buf[n++] = (uint8_t) el;
      for (int q = 0; q < el; q++) {
        s_buf[n + q] = (uint8_t) rnd();
      }
      n += el;
    }
    check(s_buf, n);
  }
}

TEST_CASE("el comparador binario coincide con la referencia de flock-you",
          "[surv][ie][diff]") {
  surv_ie_reset_signatures();
  run_corpus();
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_disagree, s_first);
  // Si el corpus dejara de producir coincidencias positivas, el test pasaria
  // sin comprobar nada util.
  TEST_ASSERT_GREATER_THAN_INT(1000, s_agree_true);
  TEST_ASSERT_GREATER_THAN_INT(1000, s_agree_false);
}
