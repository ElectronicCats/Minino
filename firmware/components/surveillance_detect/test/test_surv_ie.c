// SPDX-License-Identifier: GPL-3.0-or-later
// Vectores de trama. El vector base es sintetico y reproduce la firma
// documentada por DeFlockJoplin. Debe complementarse con capturas reales
// extraidas de los pcap de evidencia (Task 15) en cuanto existan.
#include <string.h>

#include "surv_ie.h"
#include "surv_test.h"

// SSID wildcard + los siete IE de la firma primaria de Flock
static const uint8_t FLOCK_IES[] = {
    0x00, 0x00,              // tag 0, len 0 (wildcard)
    0x02, 0x02, 0xaa, 0xbb,  // tag 2
    0x0c, 0x01, 0x00,        // tag 12
    0x7f, 0x08, 0,    0,    0,    0,    0,    0,    0,    0x40,  // tag 127
    0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,  // vendor LiteON
    0x2d, 0x02, 0x00, 0x00,                                // tag 45
    0xbf, 0x02, 0x00, 0x00,                                // tag 191
    0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00,  // vendor WFA
};

TEST_CASE("detecta SSID IE wildcard", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(
      1, surv_ie_is_wildcard_probe(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("un probe dirigido no es wildcard", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x04, 'c', 'a', 's', 'a'};
  TEST_ASSERT_EQUAL_INT(0, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("sin SSID IE devuelve -1", "[surv][ie]") {
  const uint8_t ies[] = {0x2d, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("la firma primaria de Flock matchea", "[surv][ie]") {
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("un probe con IE distintos no matchea", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

// Comprueba que una cola de bytes tras la firma no la rompe. Lo garantiza la
// salida temprana del recorrido al completarse la firma, no ningun reintento.
TEST_CASE("la firma matchea con 4 bytes de FCS al final", "[surv][ie]") {
  uint8_t ies[sizeof(FLOCK_IES) + 4];
  memcpy(ies, FLOCK_IES, sizeof(FLOCK_IES));
  memset(ies + sizeof(FLOCK_IES), 0xAB, 4);  // FCS basura
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("un TLV con longitud imposible no cuelga ni desborda", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0xff, 0x01, 0x02};
  (void) surv_ie_matches_flock(ies, sizeof(ies));  // no debe crashear
  TEST_PASS();
}

TEST_CASE("len 0 y puntero nulo se manejan", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(NULL, 0));
  TEST_ASSERT_FALSE(surv_ie_matches_flock(NULL, 0));
}

TEST_CASE("una firma anadida matchea sin perder la compilada", "[surv][ie]") {
  surv_ie_reset_signatures();
  const uint8_t otra_trama[] = {0x00, 0x00, 0x2d, 0x02, 0x00,
                                0x00, 0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));

  const surv_ie_tok_t otra[] = {{45, 0, {0}}, {191, 0, {0}}};
  TEST_ASSERT_TRUE(surv_ie_add_signature(otra, 2));
  TEST_ASSERT_TRUE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));
  // la compilada sigue viva
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));

  surv_ie_reset_signatures();
}

TEST_CASE("se respeta el tope de 8 firmas del overlay", "[surv][ie]") {
  surv_ie_reset_signatures();
  const surv_ie_tok_t t1[] = {{45, 0, {0}}};
  for (int i = 0; i < SURV_IE_MAX_SIGS; i++) {
    TEST_ASSERT_TRUE(surv_ie_add_signature(t1, 1));
  }
  TEST_ASSERT_FALSE(surv_ie_add_signature(t1, 1));
  surv_ie_reset_signatures();
}

TEST_CASE("se rechaza una firma con mas tokens de los permitidos",
          "[surv][ie]") {
  surv_ie_reset_signatures();
  surv_ie_tok_t demasiados[SURV_IE_MAX_TOKS + 1];
  memset(demasiados, 0, sizeof(demasiados));
  TEST_ASSERT_FALSE(surv_ie_add_signature(demasiados, SURV_IE_MAX_TOKS + 1));
  TEST_ASSERT_FALSE(surv_ie_add_signature(NULL, 3));
  TEST_ASSERT_FALSE(surv_ie_add_signature(demasiados, 0));
}

// PENDIENTE DE CAPTURA REAL: los tests de arriba usan un vector sintetico.
// Los casos de "phantom overflow" y resync de TLV que flock-you encontro en
// campo no se pueden reproducir sin tramas reales. En cuanto la Task 15
// produzca pcap de evidencia, extraer los IE de una trama tier-4 real y
// anadirla aqui como FLOCK_IES_REAL, con su propio TEST_CASE.
