/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OpenDroneID / Remote ID passive detection engine for Minino RF Spectrum.
 */

#include "rf_spectrum_drone.h"
#include <stdio.h>
#include <string.h>

/* IEEE 802.11 frame control constants */
#define FCTL_FTYPE_MGMT    0x00
#define FCTL_STYPE_BEACON  0x80
#define FCTL_STYPE_PROBE_R 0x50
#define FCTL_STYPE_ACTION  0xD0

/* OpenDroneID Standards OUIs */
static const uint8_t ASD_STAN_OUI[3] = {0xFA, 0x0B, 0xBC};
static const uint8_t FRENCH_OUI[3] = {0x6A, 0x5C, 0x35};
static const uint8_t NAN_SRV_ID[6] = {0x88, 0x69, 0x19, 0x9D, 0x92, 0x09};

bool rf_spectrum_drone_detect(const uint8_t* payload,
                              uint16_t len,
                              drone_detection_t* out_info) {
  if (payload == NULL || len < 24) {
    return false;
  }

  uint8_t fc0 = payload[0];
  uint8_t fc_type = fc0 & 0x0C;
  uint8_t fc_subtype = fc0 & 0xF0;

  /* Must be a Management Frame */
  if (fc_type != FCTL_FTYPE_MGMT) {
    return false;
  }

  /* Extract Source MAC (Transmitter Address) at byte 10 */
  const uint8_t* src_mac = &payload[10];

  /* 1. Check for NAN Action Frame (Subtype 0xD0) */
  if (fc_subtype == FCTL_STYPE_ACTION) {
    /* Action frame header starts at byte 24: Category (1) + Action (1) + OUI
     * (3) + Type (1) */
    if (len >= 30) {
      /* Search for NAN Remote ID service hash */
      for (uint16_t i = 24; i + 6 <= len; i++) {
        if (memcmp(&payload[i], NAN_SRV_ID, 6) == 0) {
          if (out_info) {
            out_info->detected = true;
            memcpy(out_info->mac, src_mac, 6);
            snprintf(out_info->uas_id, sizeof(out_info->uas_id),
                     "NAN-%02X%02X%02X", src_mac[3], src_mac[4], src_mac[5]);
          }
          return true;
        }
      }
    }
  }

  /* 2. Check for Beacons (0x80) or Probe Responses (0x50) carrying Vendor
   * Elements */
  if (fc_subtype == FCTL_STYPE_BEACON || fc_subtype == FCTL_STYPE_PROBE_R) {
    /* Fixed fields in Beacon: Timestamp (8) + Beacon Interval (2) + Cap Info
     * (2) = 12 bytes */
    uint16_t offset = 24 + 12;

    while (offset + 2 <= len) {
      uint8_t tag = payload[offset];
      uint8_t tag_len = payload[offset + 1];

      if (offset + 2 + tag_len > len) {
        break; /* Truncated element */
      }

      /* Vendor Specific Element (Tag 221 = 0xDD) */
      if (tag == 0xDD && tag_len >= 4) {
        const uint8_t* oui = &payload[offset + 2];
        uint8_t oui_type = payload[offset + 5];

        /* ASD-STAN / ASTM F3411: OUI FA:0B:BC, Type 0x0D */
        if (memcmp(oui, ASD_STAN_OUI, 3) == 0 && oui_type == 0x0D) {
          if (out_info) {
            out_info->detected = true;
            memcpy(out_info->mac, src_mac, 6);

            /* If ODID Basic ID message is present, attempt to extract drone
             * serial */
            if (tag_len >= 10) {
              /* ODID Message Header starts at offset + 7 (after msg_counter) */
              uint16_t data_off = offset + 7;
              if (data_off + 25 <= offset + 2 + tag_len) {
                /* Basic ID message: Type 0 in upper 4 bits */
                uint8_t msg_type = payload[data_off] >> 4;
                if (msg_type == 0) {
                  /* Basic ID UAS ID is at data_off + 2 (up to 20 ASCII chars)
                   */
                  char uas_tmp[21] = {0};
                  memcpy(uas_tmp, &payload[data_off + 2], 20);
                  uas_tmp[20] = '\0';
                  if (uas_tmp[0] >= 32 && uas_tmp[0] <= 126) {
                    snprintf(out_info->uas_id, sizeof(out_info->uas_id),
                             "%.20s", uas_tmp);
                    return true;
                  }
                }
              }
            }
            snprintf(out_info->uas_id, sizeof(out_info->uas_id),
                     "ODID-%02X%02X%02X", src_mac[3], src_mac[4], src_mac[5]);
          }
          return true;
        }

        /* French Remote ID: OUI 6A:5C:35 */
        if (memcmp(oui, FRENCH_OUI, 3) == 0) {
          if (out_info) {
            out_info->detected = true;
            memcpy(out_info->mac, src_mac, 6);
            snprintf(out_info->uas_id, sizeof(out_info->uas_id),
                     "FR-%02X%02X%02X", src_mac[3], src_mac[4], src_mac[5]);
          }
          return true;
        }
      }

      offset += 2 + tag_len;
    }
  }

  return false;
}
