// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fingerprint de IE. Procedencia y motivo del diseno binario: ver surv_ie.h.
#include "surv_ie.h"
#include <string.h>

#define VEN SURV_IE_VENDOR_TAG

// Firma primaria drive-testeada por DeFlockJoplin. Equivale a la cadena
// 2,12,127,221:506f9a16030103,45,191,221:0050f208000000 del original.
static const surv_ie_tok_t FLOCK_SIG[] = {
    {2, 0, {0}},
    {12, 0, {0}},
    {127, 0, {0}},
    {VEN, 7, {0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03}},
    {45, 0, {0}},
    {191, 0, {0}},
    {VEN, 7, {0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00}},
};
#define FLOCK_SIG_LEN ((int) (sizeof(FLOCK_SIG) / sizeof(FLOCK_SIG[0])))

// Firmas del overlay, ademas de la compilada: una flota puede correr versiones
// de firmware mezcladas tras una OTA parcial.
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
    if (id == SURV_IE_SSID_TAG) {
      return (elen == 0) ? 1 : 0;
    }
    ies += elen + 2;
    len -= elen + 2;
  }
  return -1;
}

// --- Parches de campo de flock-you -----------------------------------------
// Los tres existen porque las camaras reales emiten tramas que un recorrido
// estricto rechaza. Sin ellos el detector es MAS estricto que la referencia y
// deja pasar camaras autenticas, en silencio.

#define PHANTOM_SKIP_CAP 16
#define TLV_RESYNC_MAX   64
// Buffer de string de la referencia: alli fySigAppend falla cuando un token no
// cabe, y ese fallo aborta el intento entero. build_tokens lo refleja para no
// ser mas permisivos que ella en tramas largas.
#define REF_SIG_CAP 128

static bool liteon_vendor_at(const uint8_t* ies, int len, int pos) {
  return pos + 9 <= len && ies[pos] == VEN && ies[pos + 1] == 7 &&
         ies[pos + 2] == 0x50 && ies[pos + 3] == 0x6f && ies[pos + 4] == 0x9a;
}

static bool phantom_liteon_ahead(const uint8_t* ies, int len, int pos) {
  int end = pos + 2 + 32;
  if (end > len - 1) {
    end = len - 1;
  }
  for (int j = pos + 2; j < end; j++) {
    if (liteon_vendor_at(ies, len, j)) {
      return true;
    }
  }
  return false;
}

// Longitud declarada que se sale del buffer pero huele a desalineo del driver
// (tag 64 / len 128) con el vendor de LiteON aun por delante.
static bool is_phantom_overflow(const uint8_t* ies,
                                int len,
                                uint8_t id,
                                int elen,
                                int i) {
  if (i + 2 + elen <= len) {
    return false;
  }
  if (elen > 200) {
    return true;
  }
  return id == 64 && elen == 128 && phantom_liteon_ahead(ies, len, i);
}

// Tras un fallo de parseo, avanzar hasta encontrar una cabecera de IE
// plausible.
static int tlv_resync(const uint8_t* ies, int len, int start) {
  int end = start + TLV_RESYNC_MAX;
  if (end > len - 1) {
    end = len - 1;
  }
  for (int j = start; j < end; j++) {
    int elen = (int) ies[j + 1];
    if (elen <= 200 && j + 2 + elen <= len) {
      return j;
    }
  }
  return -1;
}

// Recorre los TLV produciendo tokens. Devuelve el numero de tokens, o -1 si el
// recorrido fallo, la firma no cabe en `cap` tokens o la trama supera el
// equivalente del buffer de string de 128 caracteres de la referencia.
static int build_tokens(const uint8_t* ies,
                        int len,
                        surv_ie_tok_t* out,
                        int cap) {
  if (ies == NULL || len < 2 || out == NULL) {
    return -1;
  }
  int i = 0;
  int n = 0;
  uint8_t phantom_skips = 0;
  // Espejo del buffer de string de la referencia: alli fySigAppend falla
  // cuando un token no cabe en 128 caracteres y aborta el intento entero
  // (lo que a su vez aborta el match; no hay "parcial se ignora"). Sin este
  // limite seriamos mas permisivos que ella en tramas cuya firma no cabe.
  // Ver SURV_IE_WALK_CAP en surv_ie.h: 64 tokens caben siempre que la firma
  // quepa en el presupuesto, asi que `cap` nunca se alcanza antes que ese
  // limite.
  int slen = 0;
  // Coste en caracteres del token tal como la referencia lo serializa: para
  // vendor "221:" + hex, para el resto los digitos decimales del tag.

  while (i + 2 <= len) {
    uint8_t id = ies[i];
    int elen = (int) ies[i + 1];

    if (i + 2 + elen > len) {
      if (phantom_skips < PHANTOM_SKIP_CAP &&
          is_phantom_overflow(ies, len, id, elen, i)) {
        phantom_skips++;
        i += 2;
        continue;
      }
      int j = tlv_resync(ies, len, i);
      if (j > i) {
        i = j;
        continue;
      }
      return -1;
    }

    i += 2;

    if (id == SURV_IE_SSID_TAG) {
      if (elen == 0) {
        while (i + 2 <= len && ies[i] == 0 && ies[i + 1] == 0) {
          i += 2;
        }
      } else {
        i += elen;
      }
      continue;
    }

    if (n >= cap) {
      return -1;  // la referencia falla igual al desbordar su buffer de string
    }
    // Separador y token, en el mismo orden y con los mismos limites que
    // fySigAppend de la referencia (la coma primero si no es el primer token,
    // y >= cap rechaza en ambos pasos).
    if (slen != 0) {
      if (slen + 1 >= REF_SIG_CAP) {
        return -1;
      }
      slen += 1;
    }
    int plen = (id == VEN && elen >= 4)
                   ? 4 + 2 * (elen < SURV_IE_VENDOR_MAX ? elen
                                                       : SURV_IE_VENDOR_MAX)
                   : (id >= 100 ? 3 : (id >= 10 ? 2 : 1));
    if (slen + plen >= REF_SIG_CAP) {
      return -1;
    }
    slen += plen;

    memset(&out[n], 0, sizeof(out[n]));
    out[n].tag = id;
    if (id == VEN && elen >= 4) {
      int take = elen < SURV_IE_VENDOR_MAX ? elen : SURV_IE_VENDOR_MAX;
      out[n].vlen = (uint8_t) take;
      memcpy(out[n].vendor, ies + i, (size_t) take);
    }
    n++;
    i += elen;
  }
  return n;
}

static bool tok_eq(const surv_ie_tok_t* a, const surv_ie_tok_t* b) {
  if (a->tag != b->tag || a->vlen != b->vlen) {
    return false;
  }
  return a->vlen == 0 || memcmp(a->vendor, b->vendor, a->vlen) == 0;
}

// Indice del primer token vendor de la firma, o -1. Es el ancla sobre la que la
// referencia canonicaliza: lo que venga ANTES puede haberse perdido por
// desalineo de parseo, asi que no se exige.
static int sig_anchor_index(const surv_ie_tok_t* sig, int sig_len) {
  for (int k = 0; k < sig_len; k++) {
    if (sig[k].tag == VEN && sig[k].vlen > 0) {
      return k;
    }
  }
  return -1;
}

static bool match_tokens(const surv_ie_tok_t* t,
                         int n,
                         const surv_ie_tok_t* sig,
                         int sig_len) {
  if (n == sig_len) {
    int k = 0;
    while (k < n && tok_eq(&t[k], &sig[k])) {
      k++;
    }
    if (k == n) {
      return true;
    }
  }
  // Coincidencia anclada: equivale a la canonicalizacion del original, que
  // reescribe el prefijo cuando encuentra el vendor de LiteON.
  int a = sig_anchor_index(sig, sig_len);
  if (a < 0) {
    return false;
  }
  // La referencia solo canonicaliza si la cadena NO empieza ya por el prefijo
  // canonico ("2,12,127,"): con ese prefijo se queda como esta y exige
  // igualdad exacta. Eso hace que un tag sobrante DENTRO del prefijo la rompa
  // mientras uno DELANTE no, lo cual es una inconsistencia del original -mismo
  // caso, veredictos opuestos segun donde caiga el tag-. Se reproduce a
  // proposito: el comportamiento validado en carretera es el suyo, no el
  // "coherente". Quitar este bloque nos vuelve mas permisivos que la
  // referencia; el test diferencial lo detectaria de inmediato.
  if (n >= a) {
    int k = 0;
    while (k < a && tok_eq(&t[k], &sig[k])) {
      k++;
    }
    if (k == a) {
      return false;  // ya se probo la igualdad exacta arriba y fallo
    }
  }
  int tail = sig_len - a;
  if (n < tail) {
    return false;
  }
  int start = n - tail;
  for (int k = 0; k < tail; k++) {
    if (!tok_eq(&t[start + k], &sig[a + k])) {
      return false;
    }
  }
  return true;
}

static bool matches_at_len(const uint8_t* ies, int len) {
  surv_ie_tok_t toks[SURV_IE_WALK_CAP];
  int n = build_tokens(ies, len, toks, SURV_IE_WALK_CAP);
  if (n <= 0) {
    return false;
  }
  if (match_tokens(toks, n, FLOCK_SIG, FLOCK_SIG_LEN)) {
    return true;
  }
  for (uint8_t s = 0; s < s_extra_count; s++) {
    if (match_tokens(toks, n, s_extra[s], (int) s_extra_len[s])) {
      return true;
    }
  }
  return false;
}

bool surv_ie_matches_flock(const uint8_t* ies, int len) {
  if (ies == NULL || len < 2) {
    return false;
  }
  if (matches_at_len(ies, len)) {
    return true;
  }
  // Reintento sin los 4 bytes de FCS: el driver a veces los entrega, y con la
  // regla de "la firma termina donde termina la lista" una cola de FCS anade
  // tokens y rompe la coincidencia. La referencia hace el mismo reintento.
  if (len > 4 && matches_at_len(ies, len - 4)) {
    return true;
  }
  return false;
}
