// SPDX-License-Identifier: GPL-3.0-or-later
//
// IE fingerprinting. Provenance and rationale for the binary design: see
// surv_ie.h.
#include "surv_ie.h"
#include <stdlib.h>
#include <string.h>

#define VEN SURV_IE_VENDOR_TAG

// Primary signature drive-tested by DeFlockJoplin. Equivalent to the string
// 2,12,127,221:506f9a16030103,45,191,221:0050f208000000 in the original.
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

// Overlay signatures, in addition to the compiled-in one: a fleet may run
// mixed firmware versions after a partial OTA.
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

// --- Field patches from flock-you -------------------------------------------
// All three exist because real cameras emit frames that a strict walk rejects.
// Without them the detector is STRICTER than the reference and silently misses
// genuine cameras.

#define PHANTOM_SKIP_CAP 16
#define TLV_RESYNC_MAX   64
// Reference string buffer: fySigAppend fails when a token does not fit, and
// that failure aborts the entire attempt. build_tokens mirrors this so we are
// not more permissive than the reference on long frames.
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

// Declared length that overflows the buffer but looks like driver misalignment
// (tag 64 / len 128) with the LiteON vendor IE still ahead.
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

// After a parse failure, advance until finding a plausible IE header.
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

// Walks TLVs producing tokens. Returns the number of tokens, or -1 if the walk
// failed, the signature does not fit in `cap` tokens, or the frame exceeds the
// equivalent of the reference's 128-character string buffer.
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
  // Mirror of the reference's string buffer: fySigAppend fails when a token
  // does not fit in 128 characters and aborts the entire attempt (which in
  // turn aborts the match; there is no "partial is ignored"). Without this
  // limit we would be more permissive than the reference on frames whose
  // signature does not fit. See SURV_IE_WALK_CAP in surv_ie.h: 64 tokens
  // always fit whenever the signature fits in the budget, so `cap` is never
  // reached before that limit.
  int slen = 0;
  // Character cost of the token as serialized by the reference: for vendor
  // "221:" + hex, otherwise the decimal digits of the tag.

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
      return -1;  // reference fails the same way when overflowing its string
                  // buffer
    }
    // Separator and token, in the same order and with the same limits as the
    // reference's fySigAppend (comma first if not the first token, and >= cap
    // rejects at both steps).
    if (slen != 0) {
      if (slen + 1 >= REF_SIG_CAP) {
        return -1;
      }
      slen += 1;
    }
    int plen =
        (id == VEN && elen >= 4)
            ? 4 + 2 * (elen < SURV_IE_VENDOR_MAX ? elen : SURV_IE_VENDOR_MAX)
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

// Index of the first vendor token in the signature, or -1. This is the anchor
// on which the reference canonicalizes: anything BEFORE it may have been lost
// due to parse misalignment, so it is not required.
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
  // Anchored match: equivalent to the original's canonicalization, which
  // rewrites the prefix when it finds the LiteON vendor IE.
  int a = sig_anchor_index(sig, sig_len);
  if (a < 0) {
    return false;
  }
  // The reference only canonicalizes if the string does NOT already start with
  // the canonical prefix ("2,12,127,"): with that prefix it stays as-is and
  // requires exact equality. That means an extra tag INSIDE the prefix breaks
  // it while one BEFORE it does not, which is an inconsistency in the original
  // (same case, opposite verdicts depending on where the tag lands). We
  // reproduce this on purpose: the road-validated behavior is theirs, not the
  // "consistent" one. Removing this block makes us more permissive than the
  // reference; the differential test would catch it immediately.
  if (n >= a) {
    int k = 0;
    while (k < a && tok_eq(&t[k], &sig[k])) {
      k++;
    }
    if (k == a) {
      return false;  // exact equality was already tested above and failed
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
  surv_ie_tok_t* toks = malloc(SURV_IE_WALK_CAP * sizeof(surv_ie_tok_t));
  if (toks == NULL) {
    return false;
  }
  int n = build_tokens(ies, len, toks, SURV_IE_WALK_CAP);
  if (n <= 0) {
    free(toks);
    return false;
  }
  if (match_tokens(toks, n, FLOCK_SIG, FLOCK_SIG_LEN)) {
    free(toks);
    return true;
  }
  for (uint8_t s = 0; s < s_extra_count; s++) {
    if (match_tokens(toks, n, s_extra[s], (int) s_extra_len[s])) {
      free(toks);
      return true;
    }
  }
  free(toks);
  return false;
}

bool surv_ie_matches_flock(const uint8_t* ies, int len) {
  if (ies == NULL || len < 2) {
    return false;
  }
  if (matches_at_len(ies, len)) {
    return true;
  }
  // Retry without the 4 FCS bytes: the driver sometimes includes them, and
  // under the "signature ends where the list ends" rule a trailing FCS adds
  // tokens and breaks the match. The reference performs the same retry.
  if (len > 4 && matches_at_len(ies, len - 4)) {
    return true;
  }
  return false;
}
