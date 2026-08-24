// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parser del overlay de firmas de microSD. Formato de linea, uno por
// entrada:
//   +oui,aa:bb:cc,<clase>,<puntos>,<tier>
//   -oui,aa:bb:cc
//   +ssid,<substring>,<clase>,<puntos>
//   +blename,<substring>,<clase>,<puntos>
//   +uuid,<hex16>,<clase>,<puntos>
//   +iesig,<tok>[,<tok>...]      tok = "<tag>" o "<tag>:<14 hex>" (vendor)
//   # comentario / linea vacia -> se ignoran, no cuentan como saltadas
//
// Recibe texto ya en memoria (no una ruta): leer el archivo es
// responsabilidad de la app (Task 17), lo que deja este parser 100%
// testeable en host. Parsea linea a linea con strtok_r sobre una copia
// acotada, sin malloc.
#include "surv_overlay.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surv_ie.h"
#include "surv_signatures.h"

// MAX_ADD/_KW/_UUID se derivan de las constantes de surv_signatures.h: son
// las mismas que dimensionan los arreglos efectivos alli. Definirlas por
// separado en cada archivo dejaria la sincronizacion librada a la
// convencion en vez de al compilador.
#define MAX_ADD      SURV_OVERLAY_MAX_OUIS
#define MAX_REMOVE   64
#define MAX_ADD_KW   SURV_OVERLAY_MAX_KWS
#define MAX_ADD_UUID SURV_OVERLAY_MAX_UUIDS
#define MAX_LINE     128
#define KW_MAX_LEN   24

static surv_oui_entry_t s_add[MAX_ADD];
static uint16_t s_add_count;
static uint8_t s_remove[MAX_REMOVE][3];
static uint16_t s_remove_count;
static surv_kw_entry_t s_add_kw[MAX_ADD_KW];
static char s_kw_pool[MAX_ADD_KW][KW_MAX_LEN];
static uint16_t s_add_kw_count;
static surv_uuid_entry_t s_add_uuid[MAX_ADD_UUID];
static uint16_t s_add_uuid_count;

void surv_overlay_reset(void) {
  s_add_count = 0;
  s_remove_count = 0;
  s_add_kw_count = 0;
  s_add_uuid_count = 0;
  surv_signatures_build_effective(NULL, 0, NULL, 0);
  surv_signatures_build_effective_kws(NULL, 0);
  surv_signatures_build_effective_uuids(NULL, 0);
  surv_ie_reset_signatures();
}

static bool parse_oui(const char* s, uint8_t out[3]) {
  unsigned a, b, c;
  if (sscanf(s, "%2x:%2x:%2x", &a, &b, &c) != 3) {
    return false;
  }
  if (strlen(s) < 8) {
    return false;
  }
  out[0] = (uint8_t) a;
  out[1] = (uint8_t) b;
  out[2] = (uint8_t) c;
  return true;
}

// "45" -> {45,0}; "221:506f9a16030103" -> {221,7,{0x50,0x6f,...}}
static bool parse_ie_token(const char* s, surv_ie_tok_t* out) {
  memset(out, 0, sizeof(*out));
  if (s == NULL || *s == '\0') {
    return false;
  }
  char* end = NULL;
  long tag = strtol(s, &end, 10);
  // strtol y no atoi, y se rechaza el tag 0. atoi("xyz") devuelve 0 en
  // silencio, y un token con tag 0 es indistinguible del elemento SSID:
  // tokens_match_sig lo salta ANTES de compararlo contra la firma, asi que la
  // firma nunca se completa y queda muerta -ocupando ademas uno de los 8
  // huecos de overlay-. Mismo modo de fallo que el token 221 sin payload, y
  // ninguno de los dos avisa de nada.
  if (end == s || (*end != '\0' && *end != ':') || tag < 1 || tag > 255) {
    return false;
  }
  const char* colon = strchr(s, ':');
  out->tag = (uint8_t) tag;
  if (colon == NULL) {
    if (out->tag == SURV_IE_VENDOR_TAG) {
      // El matcher decide comparar bytes de vendor mirando solo tag==221,
      // nunca vlen (vlen es vestigial). Un 221 pelado se "parsearia" bien y
      // despues jamas haria match con nada, en silencio. Se rechaza aqui
      // para que cuente como saltado en vez de ser una firma muerta.
      return false;
    }
    return true;
  }
  const char* hex = colon + 1;
  size_t hl = strlen(hex);
  if (hl != 14) {  // exactamente 7 bytes de vendor
    return false;
  }
  for (int i = 0; i < 7; i++) {
    unsigned b;
    if (sscanf(hex + i * 2, "%2x", &b) != 1) {
      return false;
    }
    out->vendor[i] = (uint8_t) b;
  }
  out->vlen = 7;
  return true;
}

static surv_class_t parse_class(const char* s) {
  if (strcmp(s, "flock") == 0)
    return SURV_CLASS_FLOCK;
  if (strcmp(s, "alpr") == 0)
    return SURV_CLASS_ALPR;
  if (strcmp(s, "cam") == 0)
    return SURV_CLASS_CAM;
  if (strcmp(s, "axon") == 0)
    return SURV_CLASS_AXON;
  if (strcmp(s, "glasses") == 0)
    return SURV_CLASS_GLASSES;
  return SURV_CLASS_NONE;
}

static bool handle_line(char* line, surv_overlay_stats_t* st) {
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line == '\0' || *line == '#' || *line == '\r') {
    return true;  // ignorada, no cuenta como saltada
  }
  char sign = *line++;
  char* save = NULL;
  char* kind = strtok_r(line, ",", &save);
  char* value = strtok_r(NULL, ",", &save);
  if (kind == NULL || value == NULL) {
    st->skipped++;
    return true;
  }
  if (strcmp(kind, "ssid") == 0 || strcmp(kind, "blename") == 0) {
    char* klass_s = strtok_r(NULL, ",", &save);
    char* points_s = strtok_r(NULL, ",\n\r", &save);
    surv_class_t k = (klass_s != NULL) ? parse_class(klass_s) : SURV_CLASS_NONE;
    if (k == SURV_CLASS_NONE || points_s == NULL || sign != '+' ||
        s_add_kw_count >= MAX_ADD_KW || strlen(value) >= sizeof(s_kw_pool[0])) {
      st->skipped++;
      return true;
    }
    // La copia al pool es necesaria: `value` apunta al buffer de linea, que se
    // reutiliza en la siguiente iteracion.
    strcpy(s_kw_pool[s_add_kw_count], value);
    s_add_kw[s_add_kw_count].kw = s_kw_pool[s_add_kw_count];
    s_add_kw[s_add_kw_count].klass = k;
    s_add_kw[s_add_kw_count].points = (uint8_t) atoi(points_s);
    s_add_kw_count++;
    st->added++;
    return true;
  }
  if (strcmp(kind, "uuid") == 0) {
    char* klass_s = strtok_r(NULL, ",", &save);
    char* points_s = strtok_r(NULL, ",\n\r", &save);
    unsigned u = 0;
    if (sign != '+' || klass_s == NULL || points_s == NULL ||
        sscanf(value, "%4x", &u) != 1 || s_add_uuid_count >= MAX_ADD_UUID) {
      st->skipped++;
      return true;
    }
    surv_class_t k = parse_class(klass_s);
    if (k == SURV_CLASS_NONE) {
      st->skipped++;
      return true;
    }
    s_add_uuid[s_add_uuid_count].uuid = (uint16_t) u;
    s_add_uuid[s_add_uuid_count].klass = k;
    s_add_uuid[s_add_uuid_count].points = (uint8_t) atoi(points_s);
    s_add_uuid[s_add_uuid_count].label = "overlay";
    s_add_uuid_count++;
    st->added++;
    return true;
  }
  if (strcmp(kind, "iesig") == 0) {
    // El resto de la linea son los tokens: "2", "12", "221:506f9a16030103"...
    // `value` es el primero; los demas salen de strtok_r.
    surv_ie_tok_t toks[SURV_IE_MAX_TOKS];
    uint8_t n = 0;
    const char* tok = value;
    while (tok != NULL) {
      if (n >= SURV_IE_MAX_TOKS) {
        // Mas tokens de los que caben: se salta la linea entera. Aplicarla
        // truncada dejaria cargada una firma que el usuario no escribio.
        st->skipped++;
        return true;
      }
      if (!parse_ie_token(tok, &toks[n])) {
        st->skipped++;
        return true;
      }
      n++;
      tok = strtok_r(NULL, ",\n\r", &save);
    }
    if (sign != '+' || n == 0 || !surv_ie_add_signature(toks, n)) {
      st->skipped++;
      return true;
    }
    st->added++;
    return true;
  }
  if (strcmp(kind, "oui") != 0) {
    st->skipped++;
    return true;
  }
  uint8_t oui[3];
  if (!parse_oui(value, oui)) {
    st->skipped++;
    return true;
  }
  if (sign == '-') {
    if (s_remove_count >= MAX_REMOVE) {
      st->skipped++;
      return true;
    }
    memcpy(s_remove[s_remove_count++], oui, 3);
    st->removed++;
    return true;
  }
  if (sign != '+') {
    st->skipped++;
    return true;
  }
  char* klass_s = strtok_r(NULL, ",", &save);
  char* points_s = strtok_r(NULL, ",", &save);
  char* tier_s = strtok_r(NULL, ",\n\r", &save);
  if (klass_s == NULL || points_s == NULL || tier_s == NULL) {
    st->skipped++;
    return true;
  }
  surv_class_t k = parse_class(klass_s);
  if (k == SURV_CLASS_NONE || s_add_count >= MAX_ADD) {
    st->skipped++;
    return true;
  }
  memcpy(s_add[s_add_count].oui, oui, 3);
  s_add[s_add_count].klass = k;
  s_add[s_add_count].points = (uint8_t) atoi(points_s);
  s_add[s_add_count].tier = (uint8_t) atoi(tier_s);
  s_add_count++;
  st->added++;
  return true;
}

surv_overlay_stats_t surv_overlay_parse(const char* text) {
  surv_overlay_stats_t st = {0, 0, 0};
  if (text == NULL) {
    return st;
  }
  char line[MAX_LINE];
  size_t li = 0;
  for (size_t i = 0;; i++) {
    char c = text[i];
    if (c == '\n' || c == '\0') {
      line[li] = '\0';
      handle_line(line, &st);
      li = 0;
      if (c == '\0') {
        break;
      }
      continue;
    }
    if (li < MAX_LINE - 1) {
      line[li++] = c;
    }
  }
  surv_signatures_build_effective(s_add, s_add_count, s_remove, s_remove_count);
  surv_signatures_build_effective_kws(s_add_kw, s_add_kw_count);
  surv_signatures_build_effective_uuids(s_add_uuid, s_add_uuid_count);
  return st;
}
