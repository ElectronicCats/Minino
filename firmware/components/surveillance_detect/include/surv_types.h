// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared types for the surveillance detector.
//
// Provenance: the surv_class_t taxonomy derives from the detection engines of
// eye-spy (simeononsecurity, Apache-2.0), specifically the DECL_DETECTOR macros
// in src/es_confidence.h. The SURV_TIER_* tier levels derive from flock-you
// (colonelpanichacks, MIT).
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SURV_CLASS_NONE = 0,
  SURV_CLASS_FLOCK,      // Flock Safety camera
  SURV_CLASS_FLOCK_MFR,  // contract manufacturer OUI, low confidence
  SURV_CLASS_ALPR,       // generic license plate reader
  SURV_CLASS_SOUNDTHINKING,
  SURV_CLASS_AXON,     // body cam
  SURV_CLASS_GLASSES,  // Ray-Ban Meta
  SURV_CLASS_CAM,      // consumer IP camera
  SURV_CLASS_AIRTAG,
  SURV_CLASS_SMARTTAG,
  SURV_CLASS_TILE,
  SURV_CLASS_APPLE_NEARBY,
  SURV_CLASS_IBEACON,
  SURV_CLASS_ODID,  // drone Remote ID
  SURV_CLASS_SKIMMER,
  SURV_CLASS_MESHCORE,
  SURV_CLASS_RAVEN,
  SURV_CLASS_PERSIST,  // unknown MAC seen repeatedly
  SURV_CLASS_MAX
} surv_class_t;

typedef enum { SURV_PROTO_BLE = 0, SURV_PROTO_WIFI } surv_proto_t;

// Radio phase for the "Scan All" mux: each window runs on its own cold boot
// (BLE or WiFi), because on the ESP32-C6 BLE re-init after a WiFi cycle
// fails (0x1) within the same session.
typedef enum {
  SURV_PHASE_BLE,
  SURV_PHASE_WIFI,
} surv_radio_phase_t;

#define SURV_TIER_SSID   0
#define SURV_TIER_ADDR13 1
#define SURV_TIER_ADDR2  2
#define SURV_TIER_PROBE  3
#define SURV_TIER_IE_SIG 4

#define SURV_RSSI_MIN (-95)

typedef struct {
  uint8_t mac[6];
  surv_class_t klass;
  uint8_t tier;
  int8_t rssi;
  uint8_t channel;  // 0 if BLE
  surv_proto_t proto;
} surv_event_t;

// Confidence ceiling: the event tier can never exceed the signature's tier.
// Used in both the WiFi path (surveillance_detect.c) and BLE (surv_radio.c).
static inline uint8_t surv_clamp_tier(uint8_t computed, uint8_t ceiling) {
  return computed > ceiling ? ceiling : computed;
}
