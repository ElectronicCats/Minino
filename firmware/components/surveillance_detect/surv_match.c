// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include <string.h>

// Prefiltro: un bit por primer octeto posible. Descarta el 99% de las tramas
// en una operación, que es lo que permite correr esto en el callback de WiFi.
static uint8_t s_first_octet_bitmap[32];
static bool s_inited;

void surv_match_init(void) {
  memset(s_first_octet_bitmap, 0, sizeof(s_first_octet_bitmap));
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    uint8_t b = t[i].oui[0];
    s_first_octet_bitmap[b >> 3] |= (uint8_t) (1u << (b & 7));
  }
  s_inited = true;
}

const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6]) {
  if (!s_inited || mac == NULL) {
    return NULL;
  }
  uint8_t b = mac[0];
  if ((s_first_octet_bitmap[b >> 3] & (uint8_t) (1u << (b & 7))) == 0) {
    return NULL;
  }
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    if (t[i].oui[0] == mac[0] && t[i].oui[1] == mac[1] &&
        t[i].oui[2] == mac[2]) {
      return &t[i];
    }
  }
  return NULL;
}
