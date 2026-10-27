// SPDX-License-Identifier: GPL-3.0-or-later
//
// Probe request Information Element fingerprinting.
//
// Provenance: the algorithm, primary signature, and malformed-frame patches
// come from colonelpanichacks/flock-you (MIT), which in turn builds on
// DeFlockJoplin's field research and Pintor & Atzori, "Analysis of
// Wi-Fi Probe Requests Towards Information Element Fingerprinting", GLOBECOM
// 2022, doi:10.1109/GLOBECOM48099.2022.10001618.
//
// Difference from the original: there the signature is built as a string with
// snprintf and compared with strcmp INSIDE the promiscuous callback. Here we
// build a token list and compare in binary, with no formatting on the hot
// path. Equivalence with the original is verified by a differential test that
// runs both algorithms over the same inputs (test/test_surv_ie_diff.c).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SURV_IE_SSID_TAG   0
#define SURV_IE_VENDOR_TAG 221
// Maximum tokens in a SINGLE signature (compiled-in or overlay). Bounds the
// s_extra storage and how many tokens an overlay +iesig line accepts.
#define SURV_IE_MAX_TOKS   16
#define SURV_IE_MAX_SIGS   8
#define SURV_IE_VENDOR_MAX 8  // reference encodes up to 8 vendor bytes
// Token limit when WALKING a frame. Not a signature limit: the reference
// builds the signature in a 128-character buffer and fails when a token does
// not fit; 64 is the maximum number of tokens that fit in those 128 characters
// (the minimum token is 1 digit + comma), so this cap is never reached before
// the reference's budget and the comparator follows it to the letter on long
// frames. A smaller cap diverges on frames with many small IEs ahead of the
// LiteON anchor.
#define SURV_IE_WALK_CAP 64

typedef struct {
  uint8_t tag;
  uint8_t vlen;  // significant vendor bytes; 0 if not a vendor IE
  uint8_t vendor[SURV_IE_VENDOR_MAX];
} surv_ie_tok_t;

// 1 = SSID IE present with length 0; 0 = present with non-zero length; -1 =
// absent.
int surv_ie_is_wildcard_probe(const uint8_t* ies, int len);

// true if the IEs match the Flock signature (compiled-in or from the overlay).
bool surv_ie_matches_flock(const uint8_t* ies, int len);

// Adds an alternative signature; false if the table is full or data is invalid.
bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count);

// Discards overlay signatures and leaves only the compiled-in signature.
void surv_ie_reset_signatures(void);
