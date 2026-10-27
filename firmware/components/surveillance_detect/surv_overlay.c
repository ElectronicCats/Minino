// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parser for the microSD signature overlay. Line format, one per entry:
//   +oui,aa:bb:cc,<class>,<points>,<tier>
//   -oui,aa:bb:cc
//   +ssid,<substring>,<class>,<points>
//   +blename,<substring>,<class>,<points>
//   +uuid,<hex16>,<class>,<points>
//   +iesig,<tok>[,<tok>...]      tok = "<tag>" or "<tag>:<14 hex>" (vendor)
//   # comment / empty line -> ignored, not counted as skipped
//
// Receives text already in memory (not a path): reading the file is the
// app's responsibility (Task 17), keeping this parser 100% host-testable.
// Parses line by line with strtok_r over a bounded copy, without malloc.
#include "surv_overlay.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surv_ie.h"
#include "surv_match.h"
#include "surv_signatures.h"

// MAX_ADD/_KW/_UUID are derived from constants in surv_signatures.h: they are
// the same constants that size the effective arrays there. Defining them
// separately in each file would leave synchronization up to convention rather
// than the compiler.
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

// Requires exactly "xx:xx:xx" (8 characters) and nothing else. sscanf with
// "%2x" alone happily accepts "aa:bb:ccdd" or a full MAC "aa:bb:cc:dd:ee:ff"
// as if they were the OUI "aa:bb:cc", silently ignoring the rest of the
// string. Since the overlay is edited by hand, accepting a full MAC where an
// OUI was expected makes the user believe they targeted a single device when
// they actually targeted an entire vendor prefix. %n measures how many
// characters the last matched field consumed: if the third octet has trailing
// garbage (e.g., "c!"), %2x only consumes the valid hex digit and %n catches
// it even when the full string is 8 characters long.
static bool parse_oui(const char* s, uint8_t out[3]) {
  if (strlen(s) != 8) {
    return false;
  }
  unsigned a, b, c;
  int consumed = 0;
  if (sscanf(s, "%2x:%2x:%2x%n", &a, &b, &c, &consumed) != 3 || consumed != 8) {
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
  // Use strtol instead of atoi, and reject tag 0. atoi("xyz") silently returns
  // 0, and a token with tag 0 is indistinguishable from the SSID element:
  // tokens_match_sig skips it BEFORE comparing against the signature, so the
  // signature never completes and remains dead while also occupying one of the
  // 8 overlay slots. Same failure mode as tag 221 without payload, neither of
  // which gives any warning.
  if (end == s || (*end != '\0' && *end != ':') || tag < 1 || tag > 255) {
    return false;
  }
  const char* colon = strchr(s, ':');
  out->tag = (uint8_t) tag;
  if (colon == NULL) {
    if (out->tag == SURV_IE_VENDOR_TAG) {
      // The matcher decides to compare vendor bytes by checking tag==221
      // alone, never vlen (vlen is vestigial). A bare 221 would "parse" fine
      // and then silently never match anything. Reject it here so it counts
      // as skipped instead of becoming a dead signature.
      return false;
    }
    return true;
  }
  const char* hex = colon + 1;
  size_t hl = strlen(hex);
  if (hl != 14) {  // exactly 7 vendor bytes
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

// Use strtol instead of atoi, for the same reason as parse_ie_token:
// atoi("xyz") silently returns 0, and a points/tier field read as 0 is
// indistinguishable from one legitimately written as "0". The entire line
// should count as skipped (st->skipped, visible on screen) rather than loading
// an inert entry that the user believes is active.
static bool parse_u8_field(const char* s, uint8_t* out) {
  if (s == NULL || *s == '\0') {
    return false;
  }
  char* end = NULL;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < 0 || v > 255) {
    return false;
  }
  *out = (uint8_t) v;
  return true;
}

static surv_class_t parse_class(const char* s) {
  if (strcmp(s, "flock") == 0)
    return SURV_CLASS_FLOCK;
  if (strcmp(s, "flock_mfr") == 0)
    return SURV_CLASS_FLOCK_MFR;
  if (strcmp(s, "alpr") == 0)
    return SURV_CLASS_ALPR;
  if (strcmp(s, "soundthinking") == 0)
    return SURV_CLASS_SOUNDTHINKING;
  if (strcmp(s, "axon") == 0)
    return SURV_CLASS_AXON;
  if (strcmp(s, "glasses") == 0)
    return SURV_CLASS_GLASSES;
  if (strcmp(s, "cam") == 0)
    return SURV_CLASS_CAM;
  if (strcmp(s, "airtag") == 0)
    return SURV_CLASS_AIRTAG;
  if (strcmp(s, "smarttag") == 0)
    return SURV_CLASS_SMARTTAG;
  if (strcmp(s, "tile") == 0)
    return SURV_CLASS_TILE;
  if (strcmp(s, "apple_nearby") == 0)
    return SURV_CLASS_APPLE_NEARBY;
  if (strcmp(s, "ibeacon") == 0)
    return SURV_CLASS_IBEACON;
  if (strcmp(s, "odid") == 0)
    return SURV_CLASS_ODID;
  if (strcmp(s, "skimmer") == 0)
    return SURV_CLASS_SKIMMER;
  if (strcmp(s, "meshcore") == 0)
    return SURV_CLASS_MESHCORE;
  if (strcmp(s, "raven") == 0)
    return SURV_CLASS_RAVEN;
  if (strcmp(s, "persist") == 0)
    return SURV_CLASS_PERSIST;
  return SURV_CLASS_NONE;
}

static bool handle_line(char* line, surv_overlay_stats_t* st) {
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line == '\0' || *line == '#' || *line == '\r') {
    return true;  // ignored, not counted as skipped
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
    uint8_t points = 0;
    if (k == SURV_CLASS_NONE || points_s == NULL || sign != '+' ||
        s_add_kw_count >= MAX_ADD_KW || strlen(value) >= sizeof(s_kw_pool[0]) ||
        !parse_u8_field(points_s, &points)) {
      st->skipped++;
      return true;
    }
    // Copying into the pool is required: `value` points into the line buffer,
    // which is reused on the next iteration.
    strcpy(s_kw_pool[s_add_kw_count], value);
    s_add_kw[s_add_kw_count].kw = s_kw_pool[s_add_kw_count];
    s_add_kw[s_add_kw_count].klass = k;
    s_add_kw[s_add_kw_count].points = points;
    s_add_kw_count++;
    st->added++;
    return true;
  }
  if (strcmp(kind, "uuid") == 0) {
    char* klass_s = strtok_r(NULL, ",", &save);
    char* points_s = strtok_r(NULL, ",\n\r", &save);
    unsigned u = 0;
    uint8_t points = 0;
    if (sign != '+' || klass_s == NULL || points_s == NULL ||
        sscanf(value, "%4x", &u) != 1 || s_add_uuid_count >= MAX_ADD_UUID ||
        !parse_u8_field(points_s, &points)) {
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
    s_add_uuid[s_add_uuid_count].points = points;
    s_add_uuid[s_add_uuid_count].label = "overlay";
    s_add_uuid_count++;
    st->added++;
    return true;
  }
  if (strcmp(kind, "iesig") == 0) {
    // The rest of the line holds the tokens: "2", "12", "221:506f9a16030103"...
    // `value` is the first one; the rest come from strtok_r.
    surv_ie_tok_t toks[SURV_IE_MAX_TOKS];
    uint8_t n = 0;
    const char* tok = value;
    while (tok != NULL) {
      if (n >= SURV_IE_MAX_TOKS) {
        // More tokens than fit: skip the entire line. Applying a truncated
        // signature would load something the user did not write.
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
  uint8_t points = 0;
  uint8_t tier = 0;
  if (klass_s == NULL || points_s == NULL || tier_s == NULL ||
      !parse_u8_field(points_s, &points) || !parse_u8_field(tier_s, &tier)) {
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
  s_add[s_add_count].points = points;
  s_add[s_add_count].tier = tier;
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
  // Rebuild the OUI prefilter bitmap so entries added by the overlay are
  // visible in surv_match_oui() (sniffer hot path).
  surv_match_rebuild_bitmap();
  return st;
}
