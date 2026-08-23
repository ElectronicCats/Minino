// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_ie.h"
#include <string.h>

// Firma primaria drive-testeada por DeFlockJoplin, en forma binaria:
// 2,12,127,221:506f9a16030103,45,191,221:0050f208000000
static const surv_ie_tok_t FLOCK_SIG[] = {
    {2, 0, {0}},
    {12, 0, {0}},
    {127, 0, {0}},
    {221, 7, {0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03}},
    {45, 0, {0}},
    {191, 0, {0}},
    {221, 7, {0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00}},
};
#define FLOCK_SIG_LEN (int) (sizeof(FLOCK_SIG) / sizeof(FLOCK_SIG[0]))

// Firmas del overlay, ademas de la compilada. Una flota de camaras puede
// correr versiones de firmware mezcladas tras una OTA parcial.
static surv_ie_tok_t s_extra[SURV_IE_MAX_SIGS][SURV_IE_MAX_TOKS];
static uint8_t s_extra_len[SURV_IE_MAX_SIGS];
static uint8_t s_extra_count;

bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count) {
  if (toks == NULL || count == 0 || count > SURV_IE_MAX_TOKS ||
      s_extra_count >= SURV_IE_MAX_SIGS) {
    return false;
  }
  memcpy(s_extra[s_extra_count], toks, (size_t) count * sizeof(surv_ie_tok_t));
  s_extra_len[s_extra_count] = count;
  s_extra_count++;
  return true;
}

void surv_ie_reset_signatures(void) {
  s_extra_count = 0;
}

int surv_ie_is_wildcard_probe(const uint8_t* ies, int len) {
  if (ies == NULL || len < 2) {
    return -1;
  }
  while (len >= 2) {
    uint8_t id = ies[0];
    uint8_t elen = ies[1];
    if ((int) elen + 2 > len) {
      break;
    }
    if (id == 0) {
      return (elen == 0) ? 1 : 0;
    }
    ies += elen + 2;
    len -= elen + 2;
  }
  return -1;
}

// Recorre los IE y los compara contra el patron token a token. Devuelve
// true solo si consume el patron completo sin sobrantes significativos.
static bool tokens_match_sig(const uint8_t* ies,
                             int len,
                             const surv_ie_tok_t* sig,
                             int sig_len) {
  int i = 0;
  int t = 0;
  while (i + 2 <= len && t < sig_len) {
    uint8_t id = ies[i];
    int elen = ies[i + 1];
    if (i + 2 + elen > len) {
      return false;  // TLV con longitud imposible: no es nuestra firma
    }
    i += 2;
    if (id == 0) {  // el SSID se salta, como en el original
      i += elen;
      continue;
    }
    const surv_ie_tok_t* tok = &sig[t];
    if (id != tok->tag) {
      return false;
    }
    if (id == SURV_IE_VENDOR_TAG) {
      if (elen < 7 || memcmp(ies + i, tok->vendor, 7) != 0) {
        return false;
      }
    }
    t++;
    i += elen;
  }
  return t == sig_len;
}

// Prueba la firma compilada y todas las del overlay.
static bool tokens_match(const uint8_t* ies, int len) {
  if (tokens_match_sig(ies, len, FLOCK_SIG, FLOCK_SIG_LEN)) {
    return true;
  }
  for (uint8_t i = 0; i < s_extra_count; i++) {
    if (tokens_match_sig(ies, len, s_extra[i], s_extra_len[i])) {
      return true;
    }
  }
  return false;
}

bool surv_ie_matches_flock(const uint8_t* ies, int len) {
  if (ies == NULL || len < 2) {
    return false;
  }
  // NO se reintenta sin los 4 bytes de FCS, al contrario que flock-you. Alli la
  // firma se construye consumiendo la trama entera, asi que una cola de FCS la
  // rompe y el reintento es imprescindible. Aqui el recorrido termina en cuanto
  // la firma se completa (t == sig_len), de modo que cualquier cola posterior
  // -FCS incluido- se ignora sola.
  //
  // Un reintento con len-4 seria codigo muerto demostrable: el unico chequeo
  // sensible a `len` (i + 2 + elen > len) solo se vuelve MAS estricto al
  // acortar el buffer, asi que si el intento primario fallo, el reintento
  // falla tambien. Codigo muerto que aparenta protegernos es peor que su
  // ausencia: sugiere una garantia que nadie tiene.
  return tokens_match(ies, len);
}
