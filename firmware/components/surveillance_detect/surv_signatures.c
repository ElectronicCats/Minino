// SPDX-License-Identifier: GPL-3.0-or-later
//
// Procedencia de las tablas de esta unidad: eye-spy
// (github.com/simeononsecurity/eye-spy, Apache-2.0), src/es_detect.h.
// Los OUIs de Flock Safety fueron recopilados por @NitekryDPaul
// (nite-oui-collection) y por DeFlockJoplin, y llegaron a eye-spy vía
// flock-you (MIT); se transcriben aqui con la misma clase y peso que
// tienen en la tabla de origen.
#include "surv_signatures.h"
#include <string.h>

static const surv_oui_entry_t OUIS[] = {
    // --- Flock Safety (@NitekryDPaul, dataset en modo promiscuo, 25 entradas)
    // --- clase FLOCK, +5, techo tier 4 ---
    {{0x70, 0xc9, 0x4e}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x91, 0x80}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xd8, 0xf3, 0xbc}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x80, 0x30, 0x49}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xb8, 0x35, 0x32}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x14, 0x5a, 0xfc}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x74, 0x4c, 0xa1}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x08, 0x3a, 0x88}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x9c, 0x2f, 0x9d}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xc0, 0x35, 0x32}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x94, 0x08, 0x53}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xe4, 0xaa, 0xea}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x24, 0xb2, 0xb9}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xb8, 0x1e, 0xa4}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x70, 0x08, 0x94}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x58, 0x8e, 0x81}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xec, 0x1b, 0xbd}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x71, 0xbf}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x58, 0x00, 0xe3}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x90, 0x35, 0xea}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x5c, 0x93, 0xa2}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x64, 0x6e, 0x69}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x48, 0x27, 0xea}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xa4, 0xcf, 0x12}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xe0, 0x4f, 0x43}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- DeFlockJoplin: 12ª cámara, wildcard-probe field test. Bit LAA
    //     puesto (0x82 & 0x02) pero confirmado Flock. NO filtrar. ---
    {{0x82, 0x6b, 0xf2}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Asignación IEEE directa de Flock Safety (dougborg/PR#39) ---
    {{0xb4, 0x1e, 0x52}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- FS Ext Battery, serie de dispositivos (dougborg/PR#39, 6 entradas)
    // ---
    {{0x04, 0x0d, 0x84}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xf0, 0x82, 0xc0}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x1c, 0x34, 0xf1}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x38, 0x5b, 0x44}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x94, 0x34, 0x69}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0xb4, 0xe3, 0xf9}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Entradas legacy de eye-spy (registradas ante IEEE, se mantienen) ---
    {{0xd4, 0xbb, 0xe6}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x61, 0x05}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},

    // --- Fabricante contratista (Liteon/USI, 6) --- clase FLOCK_MFR, +2,
    // techo tier 1: hardware compartido con productos no-Flock ---
    {{0xf4, 0x6a, 0xdd},
     SURV_CLASS_FLOCK_MFR,
     2,
     SURV_TIER_ADDR13},  // Liteon Technology
    {{0xf8, 0xa2, 0xd6},
     SURV_CLASS_FLOCK_MFR,
     2,
     SURV_TIER_ADDR13},  // Liteon Technology
    {{0x00, 0xf4, 0x8d},
     SURV_CLASS_FLOCK_MFR,
     2,
     SURV_TIER_ADDR13},  // Universal Scientific Industrial (USI)
    {{0xd0, 0x39, 0x57}, SURV_CLASS_FLOCK_MFR, 2, SURV_TIER_ADDR13},  // USI
    {{0xe8, 0xd0, 0xfc}, SURV_CLASS_FLOCK_MFR, 2, SURV_TIER_ADDR13},  // USI
    {{0xe0, 0x0a, 0xf6},
     SURV_CLASS_FLOCK_MFR,
     2,
     SURV_TIER_ADDR13},  // USI (dougborg/PR#39)

    // --- SoundThinking / ShotSpotter, sensores acústicos co-desplegados
    //     con ALPR de Flock (1) --- clase SOUNDTHINKING, +4 ---
    {{0xd4, 0x11, 0xd6}, SURV_CLASS_SOUNDTHINKING, 4, SURV_TIER_ADDR2},

    // --- ALPR: Motorola Solutions / Vigilant Solutions (1) --- clase ALPR, +5
    // ---
    {{0x00, 0x0e, 0x58}, SURV_CLASS_ALPR, 5, SURV_TIER_ADDR2},

    // --- Cámaras IP de consumo (31) --- clase CAM, +3 ---
    {{0x00, 0x40, 0x8c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Axis
    {{0xac, 0xcc, 0x8e}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Axis
    {{0xb8, 0xa4, 0x4f}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Axis
    {{0x4c, 0xbd, 0x8f}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hikvision
    {{0xbc, 0xad, 0x28}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hikvision
    {{0x44, 0x19, 0xb6}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hikvision
    {{0xc4, 0x2f, 0x90}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hikvision
    {{0x28, 0x57, 0xbe}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hikvision
    {{0x90, 0x02, 0xa9}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Dahua
    {{0x3c, 0xef, 0x8c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Dahua
    {{0xe0, 0x50, 0x8b}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Dahua
    {{0x4c, 0x11, 0xbf}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Dahua
    {{0xec, 0x71, 0xdb}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Reolink
    {{0x2c, 0xaa, 0x8e}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Wyze
    {{0xd0, 0x3f, 0x27}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Wyze
    {{0x3c, 0x37, 0x86}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Arlo
    {{0x14, 0xb4, 0x84}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Arlo
    {{0xf0, 0x27, 0x65}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Ring/Nest
    {{0x18, 0xb4, 0x30}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Ring/Nest
    {{0x64, 0x16, 0x66}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Nest
    {{0x9c, 0x8e, 0xcd}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Amcrest
    {{0x90, 0xc7, 0xd8}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Amcrest
    {{0x00, 0x2b, 0x67}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Vivotek
    {{0x34, 0x40, 0xb5}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hanwha
    {{0x00, 0x09, 0x6c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Hanwha
    {{0x00, 0x40, 0x48}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // FLIR
    {{0xac, 0x3a, 0x7a}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // FLIR
    {{0x00, 0x1b, 0xc5}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Mobotix
    {{0xa8, 0x9f, 0xba}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Ubiquiti
    {{0xfc, 0xec, 0xda}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Ubiquiti
    {{0x24, 0xa4, 0x3c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Ubiquiti

    // --- Axon: OUI de MAC BLE (body cams, tasers, equipo policial) ---
    // En eye-spy vive fuera de las tablas de OUI y se compara contra la BDA
    // del advertisement. Aqui va en la misma tabla: la consultan los dos
    // caminos (WiFi por OUI de AP y BLE por OUI de la BDA).
    {{0x00, 0x25, 0xdf}, SURV_CLASS_AXON, 5, SURV_TIER_ADDR2},
};

// Tabla efectiva = base + anadidos del overlay - quitados del overlay. Vive
// en RAM porque el overlay se recarga en runtime (Task 8). Sin overlay
// cargado (s_effective_built == false) los accessors devuelven OUIS tal
// cual, asi que las Tasks 1 y 2 no se rompen.
//
// El tamano se deriva de sizeof(OUIS) en vez de un literal para no quedar
// desincronizado si la tabla base crece: un literal stale aqui desbordaria
// s_effective por un elemento en cuanto el overlay llegue a su tope.
// SURV_OVERLAY_MAX_OUIS vive en surv_signatures.h: es la misma constante que
// limita cuantos OUIs acepta el overlay en surv_overlay.c.
static surv_oui_entry_t
    s_effective[(sizeof(OUIS) / sizeof(OUIS[0])) + SURV_OVERLAY_MAX_OUIS];
static uint16_t s_effective_count;
static bool s_effective_built;

void surv_signatures_build_effective(const surv_oui_entry_t* extra,
                                     uint16_t extra_count,
                                     const uint8_t (*removed)[3],
                                     uint16_t removed_count) {
  s_effective_count = 0;
  uint16_t base_n = (uint16_t) (sizeof(OUIS) / sizeof(OUIS[0]));
  for (uint16_t i = 0; i < base_n; i++) {
    bool drop = false;
    for (uint16_t j = 0; j < removed_count; j++) {
      if (memcmp(OUIS[i].oui, removed[j], 3) == 0) {
        drop = true;
        break;
      }
    }
    if (!drop) {
      s_effective[s_effective_count++] = OUIS[i];
    }
  }
  for (uint16_t i = 0; i < extra_count; i++) {
    s_effective[s_effective_count++] = extra[i];
  }
  s_effective_built = true;
}

uint16_t surv_signatures_oui_count(void) {
  return s_effective_built ? s_effective_count
                           : (uint16_t) (sizeof(OUIS) / sizeof(OUIS[0]));
}

const surv_oui_entry_t* surv_signatures_ouis(void) {
  return s_effective_built ? s_effective : OUIS;
}

static const surv_kw_entry_t KWS[] = {
    {"flocksafety", SURV_CLASS_FLOCK, 5},
    {"flock", SURV_CLASS_FLOCK, 5},
    {"pigvision", SURV_CLASS_FLOCK, 5},
    {"penguin", SURV_CLASS_FLOCK, 5},
    {"fs ext", SURV_CLASS_FLOCK, 5},
    {"raven", SURV_CLASS_RAVEN, 5},
    // "motorola" y "lpr" estan DELIBERADAMENTE fuera, pese a venir en el
    // ALPR_SSID_KW de eye-spy: rompen el criterio de aceptacion de cero falsos
    // positivos de clase ALPR. "motorola" matchea el SSID por defecto de
    // cualquier modem de consumo Motorola/Arris, que en Mexico reparten Izzi y
    // Totalplay por millones; "lpr" matchea dentro de "HelpRouter" y de las
    // impresoras "HP_LPR_*". Quien las quiera puede reponerlas por overlay de
    // microSD.
    {"licenseplat", SURV_CLASS_ALPR, 4},
    {"plateread", SURV_CLASS_ALPR, 4},
    {"vigilant", SURV_CLASS_ALPR, 4},
    {"automate", SURV_CLASS_ALPR, 4},
    {"alpr", SURV_CLASS_ALPR, 4},
    {"hikvision", SURV_CLASS_CAM, 2},
    {"doorbell", SURV_CLASS_CAM, 2},
    {"amcrest", SURV_CLASS_CAM, 2},
    {"reolink", SURV_CLASS_CAM, 2},
    {"vivotek", SURV_CLASS_CAM, 2},
    {"mobotix", SURV_CLASS_CAM, 2},
    {"genetec", SURV_CLASS_CAM, 2},
    {"protect", SURV_CLASS_CAM, 2},
    {"hanwha", SURV_CLASS_CAM, 2},
    {"dahua", SURV_CLASS_CAM, 2},
    {"lorex", SURV_CLASS_CAM, 2},
    {"blink", SURV_CLASS_CAM, 2},
    {"ipcam", SURV_CLASS_CAM, 2},
    {"unifi", SURV_CLASS_CAM, 2},
    {"arlo", SURV_CLASS_CAM, 2},
    {"wyze", SURV_CLASS_CAM, 2},
    {"ring", SURV_CLASS_CAM, 2},
    {"nest", SURV_CLASS_CAM, 2},
    {"axis", SURV_CLASS_CAM, 2},
    {"flir", SURV_CLASS_CAM, 2},
    {"surv", SURV_CLASS_CAM, 2},
    {"cctv", SURV_CLASS_CAM, 2},
    {"nvr", SURV_CLASS_CAM, 2},
    {"dvr", SURV_CLASS_CAM, 2},
    {"cam", SURV_CLASS_CAM, 2},
};

// Gemela de s_effective/build_effective, para keywords de SSID/BLE-name.
// SURV_OVERLAY_MAX_KWS vive en surv_signatures.h.
static surv_kw_entry_t
    s_effective_kws[(sizeof(KWS) / sizeof(KWS[0])) + SURV_OVERLAY_MAX_KWS];
static uint16_t s_effective_kw_count;
static bool s_effective_kws_built;

void surv_signatures_build_effective_kws(const surv_kw_entry_t* extra,
                                         uint16_t extra_count) {
  s_effective_kw_count = 0;
  uint16_t base_n = (uint16_t) (sizeof(KWS) / sizeof(KWS[0]));
  for (uint16_t i = 0; i < base_n; i++) {
    s_effective_kws[s_effective_kw_count++] = KWS[i];
  }
  for (uint16_t i = 0; i < extra_count; i++) {
    s_effective_kws[s_effective_kw_count++] = extra[i];
  }
  s_effective_kws_built = true;
}

uint16_t surv_signatures_kw_count(void) {
  return s_effective_kws_built ? s_effective_kw_count
                               : (uint16_t) (sizeof(KWS) / sizeof(KWS[0]));
}

const surv_kw_entry_t* surv_signatures_kws(void) {
  return s_effective_kws_built ? s_effective_kws : KWS;
}

// UUIDs de 16 bits, de eye-spy (Apache-2.0) y especificaciones oficiales
static const surv_uuid_entry_t UUIDS[] = {
    {0xFD5F, SURV_CLASS_GLASSES, 5, "RayBan Meta"},
    {0xFE07, SURV_CLASS_GLASSES, 4, "Snap Spectacles"},
    {0xFFFA, SURV_CLASS_ODID, 4, "Drone ODID"},
    {0xFD5A, SURV_CLASS_SMARTTAG, 3, "SmartTag"},
    {0xFEED, SURV_CLASS_TILE, 3, "Tile"},
    {0xFEEC, SURV_CLASS_TILE, 3, "Tile"},
    {0xFE2C, SURV_CLASS_AIRTAG, 4, "Google Tracker"},
    {0xFEEB, SURV_CLASS_MESHCORE, 2, "Meshtastic"},
    {0xFFE0, SURV_CLASS_SKIMMER, 4, "BLE Serial SPP"},
    {0x1802, SURV_CLASS_AIRTAG, 3, "FindMe Beacon"},
};

// Gemela de s_effective/build_effective, para UUIDs de servicio BLE.
// SURV_OVERLAY_MAX_UUIDS vive en surv_signatures.h.
static surv_uuid_entry_t s_effective_uuids[(sizeof(UUIDS) / sizeof(UUIDS[0])) +
                                           SURV_OVERLAY_MAX_UUIDS];
static uint16_t s_effective_uuid_count;
static bool s_effective_uuids_built;

void surv_signatures_build_effective_uuids(const surv_uuid_entry_t* extra,
                                           uint16_t extra_count) {
  s_effective_uuid_count = 0;
  uint16_t base_n = (uint16_t) (sizeof(UUIDS) / sizeof(UUIDS[0]));
  for (uint16_t i = 0; i < base_n; i++) {
    s_effective_uuids[s_effective_uuid_count++] = UUIDS[i];
  }
  for (uint16_t i = 0; i < extra_count; i++) {
    s_effective_uuids[s_effective_uuid_count++] = extra[i];
  }
  s_effective_uuids_built = true;
}

uint16_t surv_signatures_uuid_count(void) {
  return s_effective_uuids_built
             ? s_effective_uuid_count
             : (uint16_t) (sizeof(UUIDS) / sizeof(UUIDS[0]));
}

const surv_uuid_entry_t* surv_signatures_uuids(void) {
  return s_effective_uuids_built ? s_effective_uuids : UUIDS;
}

// Substrings de nombre BLE, case-insensitive para detección real
static const surv_name_entry_t BLE_NAMES[] = {
    {"fs ext battery", SURV_CLASS_FLOCK, 5, "Flock Battery"},
    {"pigvision", SURV_CLASS_FLOCK, 5, "Flock"},
    {"penguin", SURV_CLASS_FLOCK, 5, "Flock"},
    {"flock", SURV_CLASS_FLOCK, 5, "Flock"},
    {"raven", SURV_CLASS_RAVEN, 5, "Raven"},
    {"meshcore-", SURV_CLASS_MESHCORE, 2, "MeshCore"},
    {"meshtastic", SURV_CLASS_MESHCORE, 2, "Meshtastic"},
    {"axon", SURV_CLASS_AXON, 5, "Axon BodyCam"},
    {"body 3", SURV_CLASS_AXON, 5, "Axon Body 3"},
    {"body 2", SURV_CLASS_AXON, 5, "Axon Body 2"},
    {"taser", SURV_CLASS_AXON, 5, "Axon Taser"},
    {"v300", SURV_CLASS_AXON, 5, "Motorola V300"},
    {"watchguard", SURV_CLASS_AXON, 5, "WatchGuard Cam"},
    {"smarttag", SURV_CLASS_SMARTTAG, 3, "SmartTag"},
    {"airtag", SURV_CLASS_AIRTAG, 4, "AirTag"},
    {"tile", SURV_CLASS_TILE, 3, "Tile"},
    {"chipolo", SURV_CLASS_AIRTAG, 4, "Chipolo Tracker"},
    {"pebblebee", SURV_CLASS_AIRTAG, 4, "Pebblebee"},
    {"eufy", SURV_CLASS_AIRTAG, 3, "Eufy SmartTrack"},
    {"ray-ban", SURV_CLASS_GLASSES, 5, "RayBan Meta"},
    {"stories", SURV_CLASS_GLASSES, 4, "RayBan Stories"},
    {"spectacles", SURV_CLASS_GLASSES, 4, "Snap Spectacles"},
    {"skimmer", SURV_CLASS_SKIMMER, 5, "Card Skimmer"},
};

uint16_t surv_signatures_ble_name_count(void) {
  return (uint16_t) (sizeof(BLE_NAMES) / sizeof(BLE_NAMES[0]));
}

const surv_name_entry_t* surv_signatures_ble_names(void) {
  return BLE_NAMES;
}

// Nombres exactos de modulos comunes de skimmers y sniffers BLE/SPP
static const char* const SKIMMER_NAMES[] = {
    "HC-03",  "HC-05",  "HC-06",  "HC-08",   "HC-11",    "HC-12",
    "JDY-08", "JDY-10", "JDY-16", "JDY-18",  "JDY-23",   "JDY-30",
    "JDY-31", "JDY-33", "AT-09",  "BT05",    "MLT-BT05", "CC2540",
    "CC2541", "HM-10",  "HM-11",  "DX-BT04", "SPP-CA",   "BT-401"};

uint16_t surv_signatures_skimmer_count(void) {
  return (uint16_t) (sizeof(SKIMMER_NAMES) / sizeof(SKIMMER_NAMES[0]));
}

const char* const* surv_signatures_skimmers(void) {
  return SKIMMER_NAMES;
}
