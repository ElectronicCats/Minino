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

void surv_match_rebuild_bitmap(void) {
  memset(s_first_octet_bitmap, 0, sizeof(s_first_octet_bitmap));
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    uint8_t b = t[i].oui[0];
    s_first_octet_bitmap[b >> 3] |= (uint8_t) (1u << (b & 7));
  }
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

static char lower_ascii(char c) {
  return (c >= 'A' && c <= 'Z') ? (char) (c + 32) : c;
}

bool surv_match_contains_ci(const char* hay, const char* needle) {
  if (hay == NULL || needle == NULL || needle[0] == '\0') {
    return false;
  }
  for (size_t i = 0; hay[i] != '\0'; i++) {
    size_t j = 0;
    while (needle[j] != '\0' && hay[i + j] != '\0' &&
           lower_ascii(hay[i + j]) == lower_ascii(needle[j])) {
      j++;
    }
    if (needle[j] == '\0') {
      return true;
    }
  }
  return false;
}

const surv_kw_entry_t* surv_match_ssid(const char* ssid) {
  if (ssid == NULL || ssid[0] == '\0') {
    return NULL;
  }
  const surv_kw_entry_t* t = surv_signatures_kws();
  uint16_t n = surv_signatures_kw_count();
  for (uint16_t i = 0; i < n; i++) {
    if (surv_match_contains_ci(ssid, t[i].kw)) {
      return &t[i];
    }
  }
  return NULL;
}

static void push_hit(surv_ble_hit_t* out,
                     uint8_t* n,
                     surv_class_t k,
                     uint8_t pts,
                     const char* label) {
  if (*n >= SURV_BLE_MAX_HITS) {
    return;
  }
  for (uint8_t i = 0; i < *n; i++) {
    if (out[i].klass == k) {
      return;  // ya registrada
    }
  }
  out[*n].klass = k;
  out[*n].points = pts;
  out[*n].label = label;
  (*n)++;
}

uint8_t surv_match_ble_adv(const uint8_t* adv,
                           uint8_t adv_len,
                           surv_ble_hit_t out[SURV_BLE_MAX_HITS]) {
  uint8_t n = 0;
  if (adv == NULL || out == NULL) {
    return 0;
  }
  uint8_t off = 0;
  while (off + 1 < adv_len) {
    uint8_t len = adv[off];
    if (len == 0 || (uint16_t) (off + len + 1) > adv_len) {
      break;  // longitud imposible: advertisement truncado
    }
    uint8_t ad_type = adv[off + 1];
    const uint8_t* d = &adv[off + 2];
    uint8_t dlen = (uint8_t) (len - 1);

    if (ad_type == 0xFF && dlen >= 2) {  // manufacturer specific
      uint16_t company = (uint16_t) (d[0] | (d[1] << 8));
      if (company == 0x004C && dlen >= 3) {  // Apple
        if (d[2] == 0x12 || d[2] == 0x1E) {
          push_hit(out, &n, SURV_CLASS_AIRTAG, 4, "AirTag");
        } else if (d[2] == 0x02 && dlen >= 4 && d[3] == 0x15) {
          push_hit(out, &n, SURV_CLASS_IBEACON, 2, "iBeacon");
        } else if (d[2] == 0x07 || d[2] == 0x10 || d[2] == 0x0B ||
                   d[2] == 0x09) {
          push_hit(out, &n, SURV_CLASS_APPLE_NEARBY, 2, "Apple Dev");
        }
      } else if (company == 0x0075) {  // Samsung
        push_hit(out, &n, SURV_CLASS_SMARTTAG, 3, "SmartTag");
      } else if (company == 0x00C7) {  // Tile Inc.
        push_hit(out, &n, SURV_CLASS_TILE, 3, "Tile");
      } else if (company == 0x09C8) {  // XUNTONG, confirmado Flock (eye-spy)
        push_hit(out, &n, SURV_CLASS_FLOCK, 5, "Flock BLE");
      }
    }

    if (ad_type == 0x02 ||
        ad_type == 0x03) {  // 16-bit Service UUIDs (Complete/Incomplete)
      for (uint8_t u = 0; u + 1 < dlen; u += 2) {
        uint16_t uuid = (uint16_t) (d[u] | (d[u + 1] << 8));
        for (uint16_t i = 0; i < surv_signatures_uuid_count(); i++) {
          if (surv_signatures_uuids()[i].uuid == uuid) {
            push_hit(out, &n, surv_signatures_uuids()[i].klass,
                     surv_signatures_uuids()[i].points,
                     surv_signatures_uuids()[i].label);
          }
        }
      }
    } else if (ad_type == 0x16 && dlen >= 2) {  // Service Data (16-bit UUID)
      uint16_t uuid = (uint16_t) (d[0] | (d[1] << 8));
      for (uint16_t i = 0; i < surv_signatures_uuid_count(); i++) {
        if (surv_signatures_uuids()[i].uuid == uuid) {
          push_hit(out, &n, surv_signatures_uuids()[i].klass,
                   surv_signatures_uuids()[i].points,
                   surv_signatures_uuids()[i].label);
        }
      }
    }

    if (ad_type == 0x08 || ad_type == 0x09) {  // nombre corto / completo
      char name[32];
      uint8_t cp =
          dlen < sizeof(name) - 1 ? dlen : (uint8_t) (sizeof(name) - 1);
      memcpy(name, d, cp);
      name[cp] = '\0';
      for (uint16_t i = 0; i < surv_signatures_skimmer_count(); i++) {
        if (strcmp(name, surv_signatures_skimmers()[i]) == 0) {
          push_hit(out, &n, SURV_CLASS_SKIMMER, 5, "Skimmer");
        }
      }
      for (uint16_t i = 0; i < surv_signatures_ble_name_count(); i++) {
        if (surv_match_contains_ci(name, surv_signatures_ble_names()[i].name)) {
          push_hit(out, &n, surv_signatures_ble_names()[i].klass,
                   surv_signatures_ble_names()[i].points,
                   surv_signatures_ble_names()[i].label);
        }
      }
    }

    off = (uint8_t) (off + len + 1);
  }
  return n;
}
