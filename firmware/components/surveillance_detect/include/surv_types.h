// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tipos compartidos del detector de vigilancia.
//
// Procedencia: la taxonomia de surv_class_t deriva de los motores de deteccion
// de eye-spy (simeononsecurity, Apache-2.0), en concreto de los DECL_DETECTOR
// de src/es_confidence.h. Los niveles de tier SURV_TIER_* derivan de flock-you
// (colonelpanichacks, MIT).
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SURV_CLASS_NONE = 0,
  SURV_CLASS_FLOCK,      // cámara Flock Safety
  SURV_CLASS_FLOCK_MFR,  // OUI de fabricante contratista, baja confianza
  SURV_CLASS_ALPR,       // lector de placas genérico
  SURV_CLASS_SOUNDTHINKING,
  SURV_CLASS_AXON,     // body cam
  SURV_CLASS_GLASSES,  // Ray-Ban Meta
  SURV_CLASS_CAM,      // cámara IP de consumo
  SURV_CLASS_AIRTAG,
  SURV_CLASS_SMARTTAG,
  SURV_CLASS_TILE,
  SURV_CLASS_APPLE_NEARBY,
  SURV_CLASS_IBEACON,
  SURV_CLASS_ODID,  // drone Remote ID
  SURV_CLASS_SKIMMER,
  SURV_CLASS_MESHCORE,
  SURV_CLASS_RAVEN,
  SURV_CLASS_PERSIST,  // MAC desconocida vista repetidamente
  SURV_CLASS_MAX
} surv_class_t;

typedef enum { SURV_PROTO_BLE = 0, SURV_PROTO_WIFI } surv_proto_t;

// Fase de radio del mux de "Scan All": cada ventana corre en un boot en frio
// propio (BLE o WiFi), porque en el ESP32-C6 la re-init BLE tras un ciclo
// WiFi falla (0x1) dentro de la misma sesion.
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
  uint8_t channel;  // 0 si es BLE
  surv_proto_t proto;
} surv_event_t;

// Techo de confianza: el tier del evento nunca puede superar el tier de la
// firma. Usado tanto en la ruta WiFi (surveillance_detect.c) como en BLE
// (surv_radio.c).
static inline uint8_t surv_clamp_tier(uint8_t computed, uint8_t ceiling) {
  return computed > ceiling ? ceiling : computed;
}
