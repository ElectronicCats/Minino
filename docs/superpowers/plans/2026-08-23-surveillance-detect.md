# Detector de vigilancia para Minino — plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Añadir a Minino un detector pasivo de infraestructura de vigilancia (cámaras Flock/ALPR, body cams, rastreadores, cámaras IP, drones, skimmers) que alerta localmente y registra cada detección con GPS en microSD.

**Architecture:** Un componente `surveillance_detect` en C puro con los matchers y el motor de score aislados de todo hardware —para que corran como tests en host Linux— más una app de menú que solo hace UI y persistencia. El callback promiscuo de WiFi y el de GAP de BLE solo comparan y encolan en un ring buffer; una task aparte drena, puntúa y notifica.

**Tech Stack:** ESP-IDF v5.5.1, target `esp32c6`, C99. Unity + `test_apps` con target `linux` para los tests de host. Componentes existentes de Minino: `sd_card`, `nmea_parser`, `buzzer`, `leds`, `preferences`, `oled_screen`, `menus_module`, `espressif__pcap`.

**Spec:** `docs/superpowers/specs/2026-08-23-surveillance-detect-design.md`

**Branch:** `feat/surveillance-detect`

## Global Constraints

Todos los valores son literales del spec. Aplican a todas las tareas.

- **ESP-IDF v5.5.1**, target `esp32c6`. Los tests de host usan target `linux`.
- **Licencia GPL-3.0.** Cada archivo derivado de flock-you o eye-spy lleva cabecera con `SPDX-License-Identifier: GPL-3.0-or-later` y un aviso de procedencia citando el proyecto original y su licencia (MIT / Apache-2.0).
- **Prefijo `surv_`** en todos los símbolos del componente. Nunca `sd_` (colisiona con `sd_card`).
- **Sin `printf`, `malloc`, ni I/O de archivo en callbacks de radio.** Solo comparación y encolado.
- **Ring buffer:** 32 entradas.
- **Tabla de dispositivos:** 200 MAC máximo.
- **Score:** decay −1 cada **60 s**; cooldown de re-score **120 s** por clase; CLEAR 0–2, CAUTION 3–5, ALERT 6+.
- **Dedupe de emisión:** 5 s por MAC; un tier superior lo preempta.
- **Canales:** 11/6/1 descendente, dwell **250 ms**; en perfil `Vigilancia` se extiende a 13/8/3 cada 4 vueltas. Siempre recortado al `nchan` de la región.
- **Umbral de RSSI:** se descartan tramas y advertisements por debajo de **−95 dBm**. (Valor de `RSSI_MIN` de flock-you. El spec no lo fijaba; queda fijado aquí.)
- **CSV:** línea de hasta **200 bytes**, buffer de **200 líneas**, directorio `surveil/`.
- **pcap:** tope de **4 MB** por sesión.
- **Overlay:** topes de **256 OUIs, 64 keywords, 8 firmas IE**, más **16 UUIDs**
  (el spec no fijaba tope de UUIDs; queda fijado aquí).
- **Tabla base:** 75 OUIs (35 Flock, 6 fabricante contratista, 1 SoundThinking, 1 ALPR, 31 cámaras, 1 Axon).
  El spec dice 74 contando solo las cinco tablas WiFi de eye-spy; Axon se detecta allí
  por OUI de MAC BLE, fuera de esas tablas. Aquí va en la misma tabla unificada, que la
  consultan tanto el camino de WiFi como el de BLE.
- **No filtrar MAC localmente administradas.** `82:6b:f2` tiene ese bit puesto y es una cámara Flock confirmada.

---

## Estructura de archivos

### Componente nuevo: `firmware/components/surveillance_detect/`

| Archivo | Responsabilidad |
|---|---|
| `include/surv_types.h` | Enums (`surv_class_t`, `surv_proto_t`, `surv_tier_t`) y `surv_event_t`. Sin dependencias |
| `include/surv_signatures.h`, `surv_signatures.c` | Tabla base de OUIs, keywords, nombres BLE, UUIDs, firma IE. Accessors |
| `include/surv_match.h`, `surv_match.c` | Matchers puros: OUI con prefiltro bitmap, keyword de SSID, nombre BLE, disector de advertising |
| `include/surv_ie.h`, `surv_ie.c` | Fingerprint de Information Elements. Aislado por ser la pieza más delicada |
| `include/surv_overlay.h`, `surv_overlay.c` | Parser del overlay de microSD. Recibe texto, no toca el sistema de archivos |
| `include/surv_engine.h`, `surv_engine.c` | Score, decay, cooldown, tabla por MAC, dedupe |
| `surv_radio.c` | Planificador de fases y salto de canal. **Solo target esp32c6** |
| `include/surveillance_detect.h`, `surveillance_detect.c` | API pública, callbacks de radio, ring buffer. **Solo target esp32c6** |
| `test_apps/surv/` | Test app de Unity para host |

Los seis primeros no incluyen ninguna cabecera de ESP-IDF fuera de `stdint.h`/`stdbool.h`/`string.h`. Es la condición para que los tests corran en host.

### App nueva: `firmware/main/apps/surveillance/`

| Archivo | Responsabilidad |
|---|---|
| `surveillance_module.c/.h` | Ciclo de vida, callbacks de botones, orquestación |
| `surveillance_screens.c/.h` | Pantallas OLED |
| `surveillance_log.c/.h` | Escritura CSV, GPX y pcap a microSD |

`firmware/main/CMakeLists.txt` hace `file(GLOB_RECURSE ...)` sobre `main/`, así que **no hay que registrar los archivos nuevos**.

### Archivos existentes que se modifican

| Archivo | Cambio |
|---|---|
| `firmware/components/trackers_scanner/trackers_scanner.c` | Delega el disector a `surv_match`; pasa a scan pasivo |
| `firmware/components/bt_gattc/bt_gattc.c`, `bt_gattc.h` | Permitir scan pasivo sin cambiar el default de otros consumidores |
| `firmware/components/radio_selector/include/radio_selector.h`, `radio_selector.c` | `RADIO_SELECT_SURVEILLANCE` y su setter |
| `firmware/main/modules/menus_module/menus_include/menus.h` | Entrada de menú |
| `firmware/main/modules/settings/` | Perfil de radio, toggle de scan activo, máscara de buzzer |
| `.github/workflows/` | Workflow nuevo para tests de host |

---

## Task 1: Esqueleto del componente, tipos y test app en host

**Files:**
- Create: `firmware/components/surveillance_detect/CMakeLists.txt`
- Create: `firmware/components/surveillance_detect/include/surv_types.h`
- Create: `firmware/components/surveillance_detect/surv_signatures.c`
- Create: `firmware/components/surveillance_detect/include/surv_signatures.h`
- Create: `firmware/components/surveillance_detect/test_apps/surv/CMakeLists.txt`
- Create: `firmware/components/surveillance_detect/test_apps/surv/sdkconfig.defaults`
- Create: `firmware/components/surveillance_detect/test_apps/surv/main/CMakeLists.txt`
- Create: `firmware/components/surveillance_detect/test_apps/surv/main/test_app_main.c`
- Create: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_signatures.c`
- Create: `.github/workflows/host-tests.yml`

**Interfaces:**
- Consumes: nada.
- Produces: `surv_class_t`, `surv_proto_t`, `surv_event_t`, `SURV_TIER_*`; `surv_signatures_oui_count()`.

- [ ] **Step 1: Escribir el test que falla**

`test_apps/surv/main/test_surv_signatures.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "unity.h"
#include "surv_signatures.h"
#include "surv_types.h"

TEST_CASE("la tabla base tiene 75 OUIs", "[surv][signatures]") {
  TEST_ASSERT_EQUAL_UINT16(75, surv_signatures_oui_count());
}
```

- [ ] **Step 2: Crear el andamiaje de la test app**

`test_apps/surv/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(test_surv)
```

`test_apps/surv/sdkconfig.defaults`:

```
CONFIG_ESP_TASK_WDT_INIT=n
```

`test_apps/surv/main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "test_app_main.c"
                            "test_surv_signatures.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity surveillance_detect
                       WHOLE_ARCHIVE)
```

`test_apps/surv/main/test_app_main.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include "unity.h"
#include "unity_test_runner.h"

void app_main(void) {
  printf("Running surveillance_detect tests\n");
  unity_run_menu();
}
```

- [ ] **Step 3: Escribir los tipos**

`include/surv_types.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
// Tipos compartidos del detector de vigilancia.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SURV_CLASS_NONE = 0,
  SURV_CLASS_FLOCK,       // cámara Flock Safety
  SURV_CLASS_FLOCK_MFR,   // OUI de fabricante contratista, baja confianza
  SURV_CLASS_ALPR,        // lector de placas genérico
  SURV_CLASS_SOUNDTHINKING,
  SURV_CLASS_AXON,        // body cam
  SURV_CLASS_GLASSES,     // Ray-Ban Meta
  SURV_CLASS_CAM,         // cámara IP de consumo
  SURV_CLASS_AIRTAG,
  SURV_CLASS_SMARTTAG,
  SURV_CLASS_TILE,
  SURV_CLASS_APPLE_NEARBY,
  SURV_CLASS_IBEACON,
  SURV_CLASS_ODID,        // drone Remote ID
  SURV_CLASS_SKIMMER,
  SURV_CLASS_MESHCORE,
  SURV_CLASS_RAVEN,
  SURV_CLASS_PERSIST,     // MAC desconocida vista repetidamente
  SURV_CLASS_MAX
} surv_class_t;

typedef enum { SURV_PROTO_BLE = 0, SURV_PROTO_WIFI } surv_proto_t;

#define SURV_TIER_SSID   0
#define SURV_TIER_ADDR13 1
#define SURV_TIER_ADDR2  2
#define SURV_TIER_PROBE  3
#define SURV_TIER_IE_SIG 4

#define SURV_RSSI_MIN (-95)

typedef struct {
  uint8_t      mac[6];
  surv_class_t klass;
  uint8_t      tier;
  int8_t       rssi;
  uint8_t      channel;  // 0 si es BLE
  surv_proto_t proto;
} surv_event_t;
```

- [ ] **Step 4: Escribir la tabla base y su accessor**

`include/surv_signatures.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tablas de firmas. Procedencia:
//   - OUIs de Flock: @NitekryDPaul (nite-oui-collection) vía flock-you (MIT)
//     y DeFlockJoplin (82:6b:f2).
//   - OUIs de cámaras, ALPR, SoundThinking y firmas BLE: eye-spy (Apache-2.0).
#pragma once
#include <stdint.h>
#include "surv_types.h"

typedef struct {
  uint8_t      oui[3];
  surv_class_t klass;
  uint8_t      points;  // peso en el score global
  uint8_t      tier;    // TECHO de confianza para esta firma, no el tier del evento
} surv_oui_entry_t;

typedef struct {
  const char*  kw;
  surv_class_t klass;
  uint8_t      points;
} surv_kw_entry_t;

typedef struct {
  uint16_t     uuid;    // service UUID de 16 bits
  surv_class_t klass;
  uint8_t      points;
  const char*  label;
} surv_uuid_entry_t;

typedef struct {
  const char*  name;    // substring, case-insensitive
  surv_class_t klass;
  uint8_t      points;
  const char*  label;
} surv_name_entry_t;

uint16_t                  surv_signatures_oui_count(void);
const surv_oui_entry_t*   surv_signatures_ouis(void);
uint16_t                  surv_signatures_kw_count(void);
const surv_kw_entry_t*    surv_signatures_kws(void);
uint16_t                  surv_signatures_uuid_count(void);
const surv_uuid_entry_t*  surv_signatures_uuids(void);
uint16_t                  surv_signatures_ble_name_count(void);
const surv_name_entry_t*  surv_signatures_ble_names(void);
uint16_t                  surv_signatures_skimmer_count(void);
const char* const*        surv_signatures_skimmers(void);

// Reconstruye la tabla efectiva = base + anadidos - quitados. La llama el
// overlay (Task 8). Con extra_count y removed_count en 0 devuelve la base.
void surv_signatures_build_effective(const surv_oui_entry_t* extra,
                                     uint16_t extra_count,
                                     const uint8_t (*removed)[3],
                                     uint16_t removed_count);
void surv_signatures_build_effective_kws(const surv_kw_entry_t* extra,
                                         uint16_t extra_count);
void surv_signatures_build_effective_uuids(const surv_uuid_entry_t* extra,
                                           uint16_t extra_count);
```

`surv_signatures.c` — las 74 entradas. Extracto con la estructura exacta; las
restantes se transcriben de `eye-spy/src/es_detect.h` respetando la clase y el
peso de cada tabla de origen:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_signatures.h"

static const surv_oui_entry_t OUIS[] = {
    // --- Flock Safety (@NitekryDPaul, 25 entradas) --- clase FLOCK, +5, techo tier 4
    {{0x70, 0xc9, 0x4e}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x91, 0x80}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // ... 23 más
    // --- DeFlockJoplin: bit LAA puesto, NO filtrar ---
    {{0x82, 0x6b, 0xf2}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Asignación IEEE directa de Flock Safety ---
    {{0xb4, 0x1e, 0x52}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- FS Ext Battery (6 entradas) ---
    {{0x04, 0x0d, 0x84}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // ... 5 más
    // --- Entradas legacy de eye-spy ---
    {{0xd4, 0xbb, 0xe6}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x61, 0x05}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Fabricante contratista (Liteon/USI, 6) --- techo tier 1: hardware compartido
    {{0xf4, 0x6a, 0xdd}, SURV_CLASS_FLOCK_MFR, 2, SURV_TIER_ADDR13},
    // ... 5 más
    // --- SoundThinking / ShotSpotter (1) ---
    {{0xd4, 0x11, 0xd6}, SURV_CLASS_SOUNDTHINKING, 4, SURV_TIER_ADDR2},
    // --- ALPR: Motorola Solutions / Vigilant (1) ---
    {{0x00, 0x0e, 0x58}, SURV_CLASS_ALPR, 5, SURV_TIER_ADDR2},
    // --- Cámaras IP (31) ---
    {{0x00, 0x40, 0x8c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Axis
    // ... 30 más
    // --- Axon: OUI de MAC BLE (body cams, tasers, equipo policial) ---
    // En eye-spy vive fuera de las tablas de OUI y se compara contra la BDA del
    // advertisement. Aqui va en la misma tabla: la consultan los dos caminos.
    {{0x00, 0x25, 0xdf}, SURV_CLASS_AXON, 5, SURV_TIER_ADDR2},
};

uint16_t surv_signatures_oui_count(void) {
  return (uint16_t) (sizeof(OUIS) / sizeof(OUIS[0]));
}

const surv_oui_entry_t* surv_signatures_ouis(void) {
  return OUIS;
}
```

`CMakeLists.txt` del componente — el `if` de target es lo que permite el test en
host, copiando el patrón de `firmware/components/console/CMakeLists.txt`:

```cmake
idf_build_get_property(target IDF_TARGET)

set(srcs "surv_signatures.c"
         "surv_match.c"
         "surv_ie.c"
         "surv_overlay.c"
         "surv_engine.c")

set(reqs "")

if(NOT ${target} STREQUAL "linux")
    list(APPEND srcs "surv_radio.c" "surveillance_detect.c")
    list(APPEND reqs esp_wifi bt esp_timer)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES ${reqs})
```

Los archivos `surv_match.c`, `surv_ie.c`, `surv_overlay.c` y `surv_engine.c` se
crean en esta tarea **vacíos salvo un `#include` de su cabecera**, para que el
componente compile; su contenido llega en las tareas 2, 6, 8 y 7.

- [ ] **Step 5: Ejecutar el test en host y verificar que pasa**

```bash
cd firmware/components/surveillance_detect/test_apps/surv
idf.py --preview set-target linux
idf.py build
./build/test_surv.elf -v
```

Esperado: el test `la tabla base tiene 75 OUIs` pasa. Si falla con un conteo
distinto, faltan entradas por transcribir de `es_detect.h`.

- [ ] **Step 6: Verificar que el firmware sigue compilando para el target real**

```bash
cd firmware
idf.py set-target esp32c6
idf.py build
```

Esperado: build OK. El componente nuevo aún no lo usa nadie.

- [ ] **Step 7: Crear el workflow de tests de host**

`.github/workflows/host-tests.yml`. El workflow existente `builds.yml` solo
dispara en tags de release y no ejecuta tests, así que este es nuevo:

```yaml
name: Host tests

on:
  push:
    branches: [main, 'feat/**']
  pull_request:

jobs:
  surveillance-detect:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: 'recursive'
      - name: Run surveillance_detect host tests
        uses: espressif/esp-idf-ci-action@v1
        with:
          esp_idf_version: v5.5.1
          target: linux
          path: './firmware/components/surveillance_detect/test_apps/surv'
          command: "idf.py --preview set-target linux build && ./build/test_surv.elf -v"
```

- [ ] **Step 8: Commit**

```bash
git add firmware/components/surveillance_detect .github/workflows/host-tests.yml
git commit -m "feat(surveillance): add component skeleton, signature tables and host tests"
```

---

## Task 2: Match por OUI con prefiltro de bitmap

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`
- Create: `firmware/components/surveillance_detect/include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/test_apps/surv/main/CMakeLists.txt`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_match.c`

**Interfaces:**
- Consumes: `surv_signatures_ouis()`, `surv_signatures_oui_count()`, `surv_oui_entry_t`.
- Produces: `void surv_match_init(void)`, `const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6])`.

- [ ] **Step 1: Escribir los tests que fallan**

`test_apps/surv/main/test_surv_match.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_match.h"
#include "surv_signatures.h"
#include "unity.h"

TEST_CASE("un OUI de Flock matchea con clase FLOCK", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("una MAC desconocida no matchea", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("la MAC localmente administrada 82:6b:f2 SI matchea", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x82, 0x6b, 0xf2, 0x14, 0x07, 0x3a};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("el OUI de fabricante contratista tiene techo tier 1", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xf4, 0x6a, 0xdd, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK_MFR, e->klass);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_ADDR13, e->tier);
}

TEST_CASE("ningun OUI aparece en dos clases distintas", "[surv][match]") {
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      TEST_ASSERT_FALSE_MESSAGE(memcmp(t[i].oui, t[j].oui, 3) == 0,
                                "OUI duplicado en la tabla base");
    }
  }
}
```

Añadir `"test_surv_match.c"` a los `SRCS` de `test_apps/surv/main/CMakeLists.txt`.

- [ ] **Step 2: Ejecutar y verificar que falla**

```bash
cd firmware/components/surveillance_detect/test_apps/surv && idf.py build
```

Esperado: error de compilación, `surv_match.h` no existe.

- [ ] **Step 3: Escribir la implementación mínima**

`include/surv_match.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "surv_signatures.h"
#include "surv_types.h"

// Construye el prefiltro. Llamar una vez antes de cualquier match.
void surv_match_init(void);

// Devuelve la entrada de la tabla o NULL. Apto para llamarse desde el callback
// promiscuo: sin asignación de memoria, sin I/O.
const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6]);
```

`surv_match.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include <string.h>

// Prefiltro: un bit por primer octeto posible. Descarta el 99% de las tramas
// en una operación, que es lo que permite correr esto en el callback de WiFi.
static uint8_t s_first_octet_bitmap[32];
static bool    s_inited;

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
```

- [ ] **Step 4: Ejecutar y verificar que pasan**

```bash
cd firmware/components/surveillance_detect/test_apps/surv && idf.py build && ./build/test_surv.elf -v
```

Esperado: los cinco tests pasan. Si el de OUI duplicado falla, hay una entrada
transcrita en dos tablas distintas de `es_detect.h`: corregir la tabla, no el test.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add OUI matcher with first-octet bitmap prefilter"
```

---

## Task 3: Matchers de keyword de SSID y nombre BLE

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`, `include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/surv_signatures.c`, `include/surv_signatures.h`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_match.c`

**Interfaces:**
- Consumes: `surv_signatures_kws()`, `surv_signatures_kw_count()`.
- Produces: `const surv_kw_entry_t* surv_match_ssid(const char* ssid)`, `bool surv_match_contains_ci(const char* hay, const char* needle)`.

- [ ] **Step 1: Escribir los tests que fallan**

Añadir a `test_surv_match.c`:

```c
TEST_CASE("la busqueda de substring ignora mayusculas", "[surv][match]") {
  TEST_ASSERT_TRUE(surv_match_contains_ci("FLOCK_CAM_0032", "flock"));
  TEST_ASSERT_TRUE(surv_match_contains_ci("mi-FlOcK-red", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("floc", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci(NULL, "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("flock", NULL));
}

TEST_CASE("un SSID de Flock matchea con 5 puntos y tier 0", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("Flock_CAM_0032");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("un SSID de camara matchea con 2 puntos", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("casa-ipcam-01");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
  TEST_ASSERT_EQUAL_UINT8(2, e->points);
}

TEST_CASE("un SSID normal no matchea", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("INFINITUM1234"));
}
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `idf.py build` en `test_apps/surv`. Esperado: `surv_match_ssid` no declarada.

- [ ] **Step 3: Añadir la tabla de keywords**

En `surv_signatures.c`, transcritas de `FLOCK_SSID_KW`, `ALPR_SSID_KW` y
`CAM_SSID_KW` de eye-spy. **Orden importante: de más específica a menos**, para
que "flocksafety" gane antes de que "cam" atrape un genérico:

```c
static const surv_kw_entry_t KWS[] = {
    {"flocksafety", SURV_CLASS_FLOCK, 5}, {"flock", SURV_CLASS_FLOCK, 5},
    {"pigvision", SURV_CLASS_FLOCK, 5},   {"penguin", SURV_CLASS_FLOCK, 5},
    {"fs ext", SURV_CLASS_FLOCK, 5},      {"raven", SURV_CLASS_RAVEN, 5},
    {"licenseplat", SURV_CLASS_ALPR, 4},  {"plateread", SURV_CLASS_ALPR, 4},
    {"vigilant", SURV_CLASS_ALPR, 4},     {"motorola", SURV_CLASS_ALPR, 4},
    {"automate", SURV_CLASS_ALPR, 4},     {"alpr", SURV_CLASS_ALPR, 4},
    {"lpr", SURV_CLASS_ALPR, 4},
    {"hikvision", SURV_CLASS_CAM, 2},     {"doorbell", SURV_CLASS_CAM, 2},
    {"amcrest", SURV_CLASS_CAM, 2},       {"reolink", SURV_CLASS_CAM, 2},
    {"vivotek", SURV_CLASS_CAM, 2},       {"mobotix", SURV_CLASS_CAM, 2},
    {"genetec", SURV_CLASS_CAM, 2},       {"protect", SURV_CLASS_CAM, 2},
    {"hanwha", SURV_CLASS_CAM, 2},        {"dahua", SURV_CLASS_CAM, 2},
    {"lorex", SURV_CLASS_CAM, 2},         {"blink", SURV_CLASS_CAM, 2},
    {"ipcam", SURV_CLASS_CAM, 2},         {"unifi", SURV_CLASS_CAM, 2},
    {"arlo", SURV_CLASS_CAM, 2},          {"wyze", SURV_CLASS_CAM, 2},
    {"ring", SURV_CLASS_CAM, 2},          {"nest", SURV_CLASS_CAM, 2},
    {"axis", SURV_CLASS_CAM, 2},          {"flir", SURV_CLASS_CAM, 2},
    {"surv", SURV_CLASS_CAM, 2},          {"cctv", SURV_CLASS_CAM, 2},
    {"nvr", SURV_CLASS_CAM, 2},           {"dvr", SURV_CLASS_CAM, 2},
    {"cam", SURV_CLASS_CAM, 2},
};

uint16_t surv_signatures_kw_count(void) {
  return (uint16_t) (sizeof(KWS) / sizeof(KWS[0]));
}

const surv_kw_entry_t* surv_signatures_kws(void) {
  return KWS;
}
```

- [ ] **Step 4: Implementar los matchers**

En `surv_match.c`:

```c
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
```

Declarar ambas en `surv_match.h`.

- [ ] **Step 5: Ejecutar y verificar que pasan**

Run: `idf.py build && ./build/test_surv.elf -v` en `test_apps/surv`.
Esperado: los nueve tests pasan.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add case-insensitive SSID keyword matcher"
```

---

## Task 4: Disector de advertising BLE compartido

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`, `include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/surv_signatures.c`, `include/surv_signatures.h`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_ble.c`

**Interfaces:**
- Consumes: `surv_match_contains_ci()`, `surv_signatures_uuids()`, `surv_signatures_uuid_count()`, `surv_signatures_ble_names()`, `surv_signatures_ble_name_count()`, `surv_signatures_skimmers()`, `surv_signatures_skimmer_count()` (todos declarados en Task 1).
- Produces:
  ```c
  #define SURV_BLE_MAX_HITS 4
  typedef struct {
    surv_class_t klass;
    uint8_t      points;
    const char*  label;   // "AirTag", "Axon", ... string estático
  } surv_ble_hit_t;
  uint8_t surv_match_ble_adv(const uint8_t* adv, uint8_t adv_len,
                             surv_ble_hit_t out[SURV_BLE_MAX_HITS]);
  ```

Diferencia clave con `trackers_scanner`, que hace `break` en el primer match:
esta función devuelve **hasta 4 clases** del mismo advertisement, porque un
dispositivo puede ser a la vez iBeacon y otra cosa.

- [ ] **Step 1: Escribir los tests que fallan**

`test_apps/surv/main/test_surv_ble.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include "unity.h"

TEST_CASE("detecta un AirTag por mfr data 004C subtipo 12", "[surv][ble]") {
  // len=0x1E, AD type=0xFF, company=0x004C (LE), subtipo=0x12
  const uint8_t adv[] = {0x1e, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AIRTAG, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(4, hits[0].points);
}

TEST_CASE("detecta un iBeacon por 4C 00 02 15", "[surv][ble]") {
  const uint8_t adv[] = {0x1a, 0xff, 0x4c, 0x00, 0x02, 0x15,
                         0x00, 0x00, 0x00, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_IBEACON, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(2, hits[0].points);
}

TEST_CASE("detecta Tile por service UUID FEED", "[surv][ble]") {
  // len=0x03, AD type=0x16 (service data), UUID 0xFEED little-endian
  const uint8_t adv[] = {0x03, 0x16, 0xed, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("detecta Ray-Ban Meta por service UUID FD5F", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("detecta ODID BLE por service UUID FFFA", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0xfa, 0xff};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_ODID, hits[0].klass);
}

TEST_CASE("detecta un skimmer HC-05 por nombre exacto", "[surv][ble]") {
  // len=0x06, AD type=0x09 (nombre completo), "HC-05"
  const uint8_t adv[] = {0x06, 0x09, 'H', 'C', '-', '0', '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_SKIMMER, hits[0].klass);
}

TEST_CASE("detecta Flock BLE por substring en el nombre", "[surv][ble]") {
  const uint8_t adv[] = {0x0f, 0x09, 'F', 'S', ' ', 'E', 'x', 't',
                         ' ',  'B',  'a', 't', 't', 'e', 'r', 'y'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("un advertisement truncado no desborda", "[surv][ble]") {
  // longitud declarada mayor que el buffer real
  const uint8_t adv[] = {0x1f, 0xff, 0x4c};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("un advertisement sin firma conocida devuelve 0", "[surv][ble]") {
  const uint8_t adv[] = {0x02, 0x01, 0x06};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("detecta Axon por el OUI de la MAC BLE", "[surv][ble]") {
  surv_match_init();
  const uint8_t bda[6] = {0x00, 0x25, 0xdf, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(bda);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AXON, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}
```

Añadir `"test_surv_ble.c"` a los `SRCS` del `main/CMakeLists.txt` de la test app.

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `idf.py build` en `test_apps/surv`. Esperado: `surv_match_ble_adv` no declarada.

- [ ] **Step 3: Añadir las firmas BLE a `surv_signatures.c`**

```c
// UUIDs de 16 bits, de eye-spy (Apache-2.0)
static const surv_uuid_entry_t UUIDS[] = {
    {0xFD5F, SURV_CLASS_GLASSES,  5, "RayBan Meta"},
    {0xFFFA, SURV_CLASS_ODID,     4, "Drone ODID"},
    {0xFD5A, SURV_CLASS_SMARTTAG, 3, "SmartTag"},
    {0xFEED, SURV_CLASS_TILE,     3, "Tile"},
    {0xFEEC, SURV_CLASS_TILE,     3, "Tile"},
};

// Substrings de nombre BLE, case-insensitive
static const surv_name_entry_t BLE_NAMES[] = {
    {"fs ext battery", SURV_CLASS_FLOCK,    5, "Flock Battery"},
    {"pigvision",      SURV_CLASS_FLOCK,    5, "Flock"},
    {"penguin",        SURV_CLASS_FLOCK,    5, "Flock"},
    {"flock",          SURV_CLASS_FLOCK,    5, "Flock"},
    {"raven",          SURV_CLASS_RAVEN,    5, "Raven"},
    {"meshcore-",      SURV_CLASS_MESHCORE, 2, "MeshCore"},
};

// Nombres exactos de modulos de skimmer
static const char* const SKIMMER_NAMES[] = {"HC-03", "HC-05", "HC-06"};
```

Escribir los seis accessors declarados en Task 1 (`surv_signatures_uuids`,
`surv_signatures_uuid_count`, `surv_signatures_ble_names`,
`surv_signatures_ble_name_count`, `surv_signatures_skimmers`,
`surv_signatures_skimmer_count`) con el mismo cuerpo trivial que
`surv_signatures_ouis()`.

- [ ] **Step 4: Implementar el disector**

En `surv_match.c`. Recorre TLV estándar de BLE una sola vez y acumula hits:

```c
static void push_hit(surv_ble_hit_t* out, uint8_t* n, surv_class_t k,
                     uint8_t pts, const char* label) {
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

uint8_t surv_match_ble_adv(const uint8_t* adv, uint8_t adv_len,
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
    uint8_t        ad_type = adv[off + 1];
    const uint8_t* d = &adv[off + 2];
    uint8_t        dlen = (uint8_t) (len - 1);

    if (ad_type == 0xFF && dlen >= 2) {  // manufacturer specific
      uint16_t company = (uint16_t) (d[0] | (d[1] << 8));
      if (company == 0x004C && dlen >= 3) {  // Apple
        if (d[2] == 0x12 || d[2] == 0x1E) {
          push_hit(out, &n, SURV_CLASS_AIRTAG, 4, "AirTag");
        } else if (d[2] == 0x02 && dlen >= 4 && d[3] == 0x15) {
          push_hit(out, &n, SURV_CLASS_IBEACON, 2, "iBeacon");
        } else if (d[2] == 0x07 || d[2] == 0x10) {
          push_hit(out, &n, SURV_CLASS_APPLE_NEARBY, 2, "Apple Dev");
        }
      } else if (company == 0x0075) {  // Samsung
        push_hit(out, &n, SURV_CLASS_SMARTTAG, 3, "SmartTag");
      } else if (company == 0x09C8) {  // XUNTONG, confirmado Flock (eye-spy)
        push_hit(out, &n, SURV_CLASS_FLOCK, 5, "Flock BLE");
      }
    }

    if ((ad_type == 0x02 || ad_type == 0x03 || ad_type == 0x16) && dlen >= 2) {
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
      uint8_t cp = dlen < sizeof(name) - 1 ? dlen : (uint8_t) (sizeof(name) - 1);
      memcpy(name, d, cp);
      name[cp] = '\0';
      for (uint16_t i = 0; i < surv_signatures_skimmer_count(); i++) {
        if (strcmp(name, surv_signatures_skimmers()[i]) == 0) {
          push_hit(out, &n, SURV_CLASS_SKIMMER, 5, "Skimmer");
        }
      }
      for (uint16_t i = 0; i < surv_signatures_ble_name_count(); i++) {
        if (surv_match_contains_ci(name,
                                   surv_signatures_ble_names()[i].name)) {
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
```

- [ ] **Step 5: Ejecutar y verificar que pasan**

Run: `idf.py build && ./build/test_surv.elf -v` en `test_apps/surv`.
Esperado: los nueve tests de `[surv][ble]` pasan.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add shared BLE advertising dissector"
```

---

## Task 5: Refactor de `trackers_scanner` sobre el disector compartido

Esta es la única tarea que toca código que hoy funciona en manos de usuarios.
Va temprano y aislada, y termina con verificación manual en hardware.

**Files:**
- Modify: `firmware/components/trackers_scanner/trackers_scanner.c:115-185`
- Modify: `firmware/components/trackers_scanner/CMakeLists.txt`
- Modify: `firmware/components/bt_gattc/bt_gattc.c:55-70`, `firmware/components/bt_gattc/include/bt_gattc.h`

**Interfaces:**
- Consumes: `surv_match_ble_adv()`, `surv_ble_hit_t`, `SURV_BLE_MAX_HITS`.
- Produces: `void bt_gattc_set_passive_scan(bool passive)` — afecta solo a las llamadas posteriores a `bt_gattc_set_ble_scan_params()`, no al default de otros consumidores.

- [ ] **Step 1: Añadir el modo pasivo a `bt_gattc`**

En `bt_gattc.c`, junto a la definición actual de scan params
(`bt_gattc.c:61` tiene `.scan_type = BLE_SCAN_TYPE_ACTIVE`):

```c
static bool s_passive_scan = false;

void bt_gattc_set_passive_scan(bool passive) {
  s_passive_scan = passive;
}
```

y en `bt_gattc_set_default_ble_scan_params()` sustituir el literal por:

```c
.scan_type = s_passive_scan ? BLE_SCAN_TYPE_PASSIVE : BLE_SCAN_TYPE_ACTIVE,
```

Declarar `bt_gattc_set_passive_scan` en `bt_gattc.h`.

- [ ] **Step 2: Sustituir el cuerpo de `tracker_dissector`**

En `trackers_scanner.c`, reemplazar el recorrido TLV completo (el `while` de
`tracker_dissector`) por una llamada al disector compartido, conservando la
firma de la función y el rellenado de `tracker_record`:

```c
static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record) {
  uint8_t* adv = scan_rst->scan_rst.ble_adv;
  uint8_t  adv_len = scan_rst->scan_rst.adv_data_len;

  tracker_record->is_tracker = false;

  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, adv_len, hits);

  // Esta app solo muestra rastreadores personales. El resto de clases que el
  // disector compartido reconoce (Flock, Axon, drones) las consume la app de
  // vigilancia, no esta.
  for (uint8_t i = 0; i < n; i++) {
    switch (hits[i].klass) {
      case SURV_CLASS_AIRTAG:
      case SURV_CLASS_SMARTTAG:
      case SURV_CLASS_TILE:
      case SURV_CLASS_APPLE_NEARBY:
        tracker_record->is_tracker = true;
        tracker_record->name = (char*) hits[i].label;
        tracker_record->vendor = (char*) hits[i].label;
        break;
      default:
        break;
    }
    if (tracker_record->is_tracker) {
      break;
    }
  }

  if (tracker_record->is_tracker) {
    tracker_record->rssi = scan_rst->scan_rst.rssi;
    tracker_record->adv_data_length = adv_len;
    memcpy(tracker_record->mac_address, scan_rst->scan_rst.bda, 6);
    size_t copy_len = (adv_len > sizeof(tracker_record->adv_data))
                          ? sizeof(tracker_record->adv_data)
                          : adv_len;
    memcpy(tracker_record->adv_data, adv, copy_len);
  }
}
```

Añadir `#include "surv_match.h"` y, en `trackers_scanner/CMakeLists.txt`,
`surveillance_detect` a `PRIV_REQUIRES`:

```cmake
idf_component_register(SRCS "trackers_scanner.c"
PRIV_REQUIRES bt bt_gattc surveillance_detect
INCLUDE_DIRS ".")
```

- [ ] **Step 3: Activar scan pasivo en el arranque del scanner**

En `trackers_scanner_start()`, antes de `bt_gattc_set_ble_scan_params()`:

```c
bt_gattc_set_passive_scan(true);
```

- [ ] **Step 4: Compilar para el target real**

```bash
cd firmware && idf.py set-target esp32c6 && idf.py build
```

Esperado: build OK.

- [ ] **Step 5: Verificación manual en hardware — obligatoria**

```bash
cd firmware && idf.py -p /dev/ttyACM0 flash monitor
```

Con un AirTag (o un Tile, o un teléfono Samsung con SmartTag) a menos de 2 m:

1. Menú → Bluetooth → Trackers Scan.
2. Comprobar que el dispositivo aparece en la lista con su nombre correcto.
3. Comprobar que el RSSI se actualiza al acercarlo y alejarlo.
4. Comprobar que salir con BUTTON_LEFT devuelve al menú sin colgarse.

Si cualquiera de los cuatro falla, **no continuar**: el disector compartido tiene
una regresión respecto al original y hay que corregirla aquí.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/trackers_scanner firmware/components/bt_gattc
git commit -m "refactor(trackers): use shared BLE dissector and passive scanning"
```

---

## Task 6: Fingerprint de Information Elements

La pieza más delicada del plan. Va en su propio archivo y su propio test.

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_ie.h`
- Modify: `firmware/components/surveillance_detect/surv_ie.c`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_ie.c`

**Interfaces:**
- Consumes: nada.
- Produces:
  ```c
  typedef struct { uint8_t tag; uint8_t vlen; uint8_t vendor[7]; } surv_ie_tok_t;
  int  surv_ie_is_wildcard_probe(const uint8_t* ies, int len);
  bool surv_ie_matches_flock(const uint8_t* ies, int len);
  // Anade una firma alternativa. La usa el overlay (Task 8) para seguir a las
  // camaras cuando Flock actualice su firmware por OTA: una flota puede correr
  // versiones mezcladas, asi que se admiten hasta SURV_IE_MAX_SIGS a la vez.
  // Devuelve false si la tabla esta llena.
  bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count);
  // Descarta las firmas del overlay y deja solo la compilada.
  void surv_ie_reset_signatures(void);
  ```
  `surv_ie_is_wildcard_probe` devuelve 1 (SSID IE de longitud 0), 0 (SSID IE con
  longitud) o −1 (no hay SSID IE).

- [ ] **Step 1: Escribir los tests que fallan**

`test_apps/surv/main/test_surv_ie.c`. El vector se construye para reproducir
exactamente la firma drive-testeada de flock-you
`2,12,127,221:506f9a16030103,45,191,221:0050f208000000`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
// Vectores de trama. El vector base es sintetico y reproduce la firma
// documentada por DeFlockJoplin. Debe complementarse con capturas reales
// extraidas de los pcap de evidencia (Task 15) en cuanto existan.
#include "surv_ie.h"
#include "unity.h"

// SSID wildcard + los siete IE de la firma primaria de Flock
static const uint8_t FLOCK_IES[] = {
    0x00, 0x00,                                      // tag 0, len 0 (wildcard)
    0x02, 0x02, 0xaa, 0xbb,                          // tag 2
    0x0c, 0x01, 0x00,                                // tag 12
    0x7f, 0x08, 0, 0, 0, 0, 0, 0, 0, 0x40,           // tag 127
    0xdd, 0x07, 0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03,  // vendor LiteON
    0x2d, 0x02, 0x00, 0x00,                          // tag 45
    0xbf, 0x02, 0x00, 0x00,                          // tag 191
    0xdd, 0x07, 0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00,  // vendor WFA
};

TEST_CASE("detecta SSID IE wildcard", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(1, surv_ie_is_wildcard_probe(FLOCK_IES,
                                                     sizeof(FLOCK_IES)));
}

TEST_CASE("un probe dirigido no es wildcard", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x04, 'c', 'a', 's', 'a'};
  TEST_ASSERT_EQUAL_INT(0, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("sin SSID IE devuelve -1", "[surv][ie]") {
  const uint8_t ies[] = {0x2d, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("la firma primaria de Flock matchea", "[surv][ie]") {
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("un probe con IE distintos no matchea", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("la firma matchea con 4 bytes de FCS al final", "[surv][ie]") {
  uint8_t ies[sizeof(FLOCK_IES) + 4];
  memcpy(ies, FLOCK_IES, sizeof(FLOCK_IES));
  memset(ies + sizeof(FLOCK_IES), 0xAB, 4);  // FCS basura
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("un TLV con longitud imposible no cuelga ni desborda", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0xff, 0x01, 0x02};
  (void) surv_ie_matches_flock(ies, sizeof(ies));  // no debe crashear
  TEST_PASS();
}

TEST_CASE("len 0 y puntero nulo se manejan", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(NULL, 0));
  TEST_ASSERT_FALSE(surv_ie_matches_flock(NULL, 0));
}

TEST_CASE("una firma anadida matchea sin perder la compilada", "[surv][ie]") {
  surv_ie_reset_signatures();
  const uint8_t otra_trama[] = {0x00, 0x00, 0x2d, 0x02, 0x00, 0x00,
                                0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));

  const surv_ie_tok_t otra[] = {{45, 0, {0}}, {191, 0, {0}}};
  TEST_ASSERT_TRUE(surv_ie_add_signature(otra, 2));
  TEST_ASSERT_TRUE(surv_ie_matches_flock(otra_trama, sizeof(otra_trama)));
  // la compilada sigue viva
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));

  surv_ie_reset_signatures();
}

TEST_CASE("se respeta el tope de 8 firmas del overlay", "[surv][ie]") {
  surv_ie_reset_signatures();
  const surv_ie_tok_t t1[] = {{45, 0, {0}}};
  for (int i = 0; i < SURV_IE_MAX_SIGS; i++) {
    TEST_ASSERT_TRUE(surv_ie_add_signature(t1, 1));
  }
  TEST_ASSERT_FALSE(surv_ie_add_signature(t1, 1));
  surv_ie_reset_signatures();
}
```

Añadir `"test_surv_ie.c"` a los `SRCS` de la test app.

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `idf.py build` en `test_apps/surv`. Esperado: `surv_ie.h` no existe.

- [ ] **Step 3: Implementar el patrón y la comparación binaria**

`include/surv_ie.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fingerprint de Information Elements de probe request.
//
// Procedencia: la firma y el manejo de tramas corruptas vienen de
// colonelpanichacks/flock-you (MIT), a su vez basado en la investigacion de
// DeFlockJoplin y en Pintor & Atzori, "Analysis of Wi-Fi Probe Requests
// Towards Information Element Fingerprinting", GLOBECOM 2022.
//
// Diferencia con el original: alli la firma se construye como string con
// snprintf y se compara con strcmp DENTRO del callback promiscuo. Aqui se
// compara token a token en binario, sin formateo en el hot path.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SURV_IE_VENDOR_TAG 221
#define SURV_IE_MAX_TOKS   16
#define SURV_IE_MAX_SIGS   8

typedef struct {
  uint8_t tag;
  uint8_t vlen;       // 0 si no es vendor
  uint8_t vendor[7];  // primeros 7 bytes del payload vendor
} surv_ie_tok_t;

// 1 = SSID IE presente con longitud 0; 0 = presente con longitud; -1 = ausente.
int surv_ie_is_wildcard_probe(const uint8_t* ies, int len);

// true si los IE reproducen la firma primaria de Flock.
bool surv_ie_matches_flock(const uint8_t* ies, int len);
```

`surv_ie.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_ie.h"
#include <string.h>

// Firma primaria drive-testeada por DeFlockJoplin, en forma binaria:
// 2,12,127,221:506f9a16030103,45,191,221:0050f208000000
static const surv_ie_tok_t FLOCK_SIG[] = {
    {2, 0, {0}},
    {12, 0, {0}},
    {127, 0, {0}},
    {221, 7, {0x50, 0x6f, 0x9a, 0x16, 0x03, 0x01, 0x03}},
    {45, 0, {0}},
    {191, 0, {0}},
    {221, 7, {0x00, 0x50, 0xf2, 0x08, 0x00, 0x00, 0x00}},
};
#define FLOCK_SIG_LEN (int) (sizeof(FLOCK_SIG) / sizeof(FLOCK_SIG[0]))

// Firmas del overlay, ademas de la compilada. Una flota de camaras puede
// correr versiones de firmware mezcladas tras una OTA parcial.
static surv_ie_tok_t s_extra[SURV_IE_MAX_SIGS][SURV_IE_MAX_TOKS];
static uint8_t       s_extra_len[SURV_IE_MAX_SIGS];
static uint8_t       s_extra_count;

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
    if (id == 0) {
      return (elen == 0) ? 1 : 0;
    }
    ies += elen + 2;
    len -= elen + 2;
  }
  return -1;
}

// Recorre los IE y los compara contra el patron token a token. Devuelve
// true solo si consume el patron completo sin sobrantes significativos.
static bool tokens_match_sig(const uint8_t* ies, int len,
                             const surv_ie_tok_t* sig, int sig_len) {
  int i = 0;
  int t = 0;
  while (i + 2 <= len && t < sig_len) {
    uint8_t id = ies[i];
    int     elen = ies[i + 1];
    if (i + 2 + elen > len) {
      return false;  // TLV con longitud imposible: no es nuestra firma
    }
    i += 2;
    if (id == 0) {  // el SSID se salta, como en el original
      i += elen;
      continue;
    }
    const surv_ie_tok_t* tok = &sig[t];
    if (id != tok->tag) {
      return false;
    }
    if (id == SURV_IE_VENDOR_TAG) {
      if (elen < 7 || memcmp(ies + i, tok->vendor, 7) != 0) {
        return false;
      }
    }
    t++;
    i += elen;
  }
  return t == sig_len;
}

// Prueba la firma compilada y todas las del overlay.
static bool tokens_match(const uint8_t* ies, int len) {
  if (tokens_match_sig(ies, len, FLOCK_SIG, FLOCK_SIG_LEN)) {
    return true;
  }
  for (uint8_t i = 0; i < s_extra_count; i++) {
    if (tokens_match_sig(ies, len, s_extra[i], s_extra_len[i])) {
      return true;
    }
  }
  return false;
}

bool surv_ie_matches_flock(const uint8_t* ies, int len) {
  if (ies == NULL || len < 2) {
    return false;
  }
  if (tokens_match(ies, len)) {
    return true;
  }
  // Reintento sin los 4 bytes de FCS: el driver a veces los entrega.
  if (len > 4 && tokens_match(ies, len - 4)) {
    return true;
  }
  return false;
}
```

- [ ] **Step 4: Ejecutar y verificar que pasan**

Run: `idf.py build && ./build/test_surv.elf -v` en `test_apps/surv`.
Esperado: los ocho tests de `[surv][ie]` pasan.

- [ ] **Step 5: Anotar la deuda de vectores reales**

Añadir al final de `test_surv_ie.c`:

```c
// PENDIENTE DE CAPTURA REAL: los tests de arriba usan un vector sintetico.
// Los casos de "phantom overflow" y resync de TLV que flock-you encontro en
// campo no se pueden reproducir sin tramas reales. En cuanto la Task 15
// produzca pcap de evidencia, extraer los IE de una trama tier-4 real y
// anadirla aqui como FLOCK_IES_REAL, con su propio TEST_CASE.
```

Esto no es un placeholder de implementación: la función queda completa y
probada. Es una nota de cobertura pendiente de material que aún no existe.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add binary IE fingerprint matcher for Flock probes"
```

---

## Task 7: Motor de score, tiers y dedupe

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_engine.h`
- Modify: `firmware/components/surveillance_detect/surv_engine.c`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_engine.c`

**Interfaces:**
- Consumes: `surv_event_t`, `surv_class_t`.
- Produces:
  ```c
  typedef void (*surv_engine_emit_cb_t)(const surv_event_t* ev, uint8_t score);
  void    surv_engine_reset(void);
  void    surv_engine_register_emit_cb(surv_engine_emit_cb_t cb);
  void    surv_engine_submit(const surv_event_t* ev, uint8_t points, uint32_t now_ms);
  void    surv_engine_tick(uint32_t now_ms);
  uint8_t surv_engine_score(void);
  uint8_t surv_engine_best_tier(const uint8_t mac[6]);
  // Rastreador de persistencia (SURV_CLASS_PERSIST, de eye-spy): una MAC
  // desconocida vista >=3 veces a lo largo de >=5 min es un posible seguidor.
  // Devuelve true la vez que dispara la puntuacion.
  bool    surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms);
  ```

El tiempo entra como parámetro `now_ms` en vez de leerse de `esp_timer`: es lo
que permite probar decay y cooldown en host sin esperar minutos reales.

- [ ] **Step 1: Escribir los tests que fallan**

`test_apps/surv/main/test_surv_engine.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_engine.h"
#include "unity.h"

static int      s_emits;
static uint8_t  s_last_score;

static void on_emit(const surv_event_t* ev, uint8_t score) {
  (void) ev;
  s_emits++;
  s_last_score = score;
}

static surv_event_t mk(uint8_t last_octet, surv_class_t k, uint8_t tier) {
  surv_event_t ev = {0};
  ev.mac[0] = 0x70; ev.mac[1] = 0xc9; ev.mac[2] = 0x4e;
  ev.mac[5] = last_octet;
  ev.klass = k;
  ev.tier = tier;
  ev.rssi = -50;
  ev.proto = SURV_PROTO_WIFI;
  return ev;
}

TEST_CASE("una deteccion suma sus puntos al score", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
}

TEST_CASE("el score decae 1 punto cada 60 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_tick(1000 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
  surv_engine_tick(1000 + 120000);
  TEST_ASSERT_EQUAL_UINT8(3, surv_engine_score());
}

TEST_CASE("el score nunca baja de cero", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_tick(600000);
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("la misma clase no re-puntua antes de 120 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t a = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t b = mk(0x02, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&a, 5, 1000);
  surv_engine_submit(&b, 5, 30000);  // otra MAC, misma clase, dentro de 120 s
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  surv_engine_submit(&b, 5, 1000 + 120001);
  TEST_ASSERT_EQUAL_UINT8(10, surv_engine_score());
}

TEST_CASE("un tier alto no se degrada con uno bajo", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 2000);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_IE_SIG, surv_engine_best_tier(hi.mac));
}

TEST_CASE("el dedupe de 5 s suprime repeticiones del mismo tier",
          "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR2);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_submit(&ev, 5, 3000);
  TEST_ASSERT_EQUAL_INT(1, s_emits);
  surv_engine_submit(&ev, 5, 6001);
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("un tier superior preempta el dedupe", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&lo, 5, 1000);
  surv_engine_submit(&hi, 5, 1500);  // dentro de los 5 s, pero tier mayor
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("una MAC desconocida vista 3 veces en 5 min puntua persistencia",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x01};
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 1000));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 120000));
  // tercera vista, y han pasado mas de 5 min desde la primera
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac, 1000 + 300001));
  TEST_ASSERT_EQUAL_UINT8(2, surv_engine_score());
}

TEST_CASE("una MAC desconocida vista 3 veces en 1 min no puntua",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x02};
  surv_engine_note_unknown(mac, 1000);
  surv_engine_note_unknown(mac, 20000);
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 40000));
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("la tabla se satura a 200 MAC sin desbordar", "[surv][engine]") {
  surv_engine_reset();
  for (int i = 0; i < 300; i++) {
    surv_event_t ev = mk((uint8_t) (i & 0xff), SURV_CLASS_CAM, SURV_TIER_ADDR2);
    ev.mac[4] = (uint8_t) (i >> 8);
    surv_engine_submit(&ev, 3, (uint32_t) (1000 + i * 10));
  }
  TEST_PASS();  // no debe crashear
}
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `idf.py build` en `test_apps/surv`. Esperado: `surv_engine.h` no existe.

- [ ] **Step 3: Implementar el motor**

`include/surv_engine.h` con las firmas del bloque Interfaces, y `surv_engine.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Motor de confianza. Combina dos modelos:
//   - tier por MAC, de flock-you (MIT): que tan seguro estoy de este equipo.
//   - score global con decay, de eye-spy (Apache-2.0): que tan vigilado esta
//     este lugar.
// Son ejes independientes: un SSID "flock" es tier 0 y a la vez 5 puntos.
#include "surv_engine.h"
#include <string.h>

#define SURV_MAX_DEVICES   200
#define SURV_DECAY_MS      60000u
#define SURV_RESCORE_MS    120000u
#define SURV_DEDUPE_MS     5000u

typedef struct {
  uint8_t  mac[6];
  uint8_t  best_tier;
  uint32_t last_emit_ms;
  bool     used;
} surv_dev_t;

static surv_dev_t            s_devs[SURV_MAX_DEVICES];
static uint16_t              s_dev_count;
static uint32_t              s_class_scored_ms[SURV_CLASS_MAX];
static bool                  s_class_seen[SURV_CLASS_MAX];
static uint8_t               s_score;
static uint32_t              s_last_decay_ms;
static surv_engine_emit_cb_t s_emit_cb;

void surv_engine_reset(void) {
  memset(s_devs, 0, sizeof(s_devs));
  memset(s_class_scored_ms, 0, sizeof(s_class_scored_ms));
  memset(s_class_seen, 0, sizeof(s_class_seen));
  s_dev_count = 0;
  s_score = 0;
  s_last_decay_ms = 0;
  s_emit_cb = NULL;
}

void surv_engine_register_emit_cb(surv_engine_emit_cb_t cb) {
  s_emit_cb = cb;
}

static surv_dev_t* find_or_add(const uint8_t mac[6]) {
  for (uint16_t i = 0; i < s_dev_count; i++) {
    if (memcmp(s_devs[i].mac, mac, 6) == 0) {
      return &s_devs[i];
    }
  }
  if (s_dev_count >= SURV_MAX_DEVICES) {
    return NULL;  // tabla llena: se sigue puntuando, no se rastrea el equipo
  }
  surv_dev_t* d = &s_devs[s_dev_count++];
  memcpy(d->mac, mac, 6);
  d->best_tier = 0;
  d->last_emit_ms = 0;
  d->used = true;
  return d;
}

uint8_t surv_engine_best_tier(const uint8_t mac[6]) {
  for (uint16_t i = 0; i < s_dev_count; i++) {
    if (memcmp(s_devs[i].mac, mac, 6) == 0) {
      return s_devs[i].best_tier;
    }
  }
  return 0;
}

void surv_engine_submit(const surv_event_t* ev, uint8_t points,
                        uint32_t now_ms) {
  if (ev == NULL || ev->klass <= SURV_CLASS_NONE ||
      ev->klass >= SURV_CLASS_MAX) {
    return;
  }
  if (ev->rssi < SURV_RSSI_MIN) {
    return;
  }

  surv_dev_t* d = find_or_add(ev->mac);
  bool tier_up = (d != NULL) && (ev->tier > d->best_tier);
  if (d != NULL && tier_up) {
    d->best_tier = ev->tier;
  }

  // Score global: cooldown por clase, no por MAC.
  bool first = !s_class_seen[ev->klass];
  if (first || (now_ms - s_class_scored_ms[ev->klass]) > SURV_RESCORE_MS) {
    uint16_t sum = (uint16_t) s_score + points;
    s_score = (uint8_t) (sum > 255 ? 255 : sum);
    s_class_scored_ms[ev->klass] = now_ms;
    s_class_seen[ev->klass] = true;
    if (s_last_decay_ms == 0) {
      s_last_decay_ms = now_ms;
    }
  }

  // Emision: dedupe de 5 s por MAC, preemptado por un tier superior.
  bool emit = true;
  if (d != NULL) {
    if (!tier_up && d->last_emit_ms != 0 &&
        (now_ms - d->last_emit_ms) < SURV_DEDUPE_MS) {
      emit = false;
    }
    if (emit) {
      d->last_emit_ms = now_ms;
    }
  }
  if (emit && s_emit_cb != NULL) {
    s_emit_cb(ev, s_score);
  }
}

void surv_engine_tick(uint32_t now_ms) {
  if (s_score == 0) {
    s_last_decay_ms = now_ms;
    return;
  }
  while (s_score > 0 && (now_ms - s_last_decay_ms) >= SURV_DECAY_MS) {
    s_score--;
    s_last_decay_ms += SURV_DECAY_MS;
  }
}

uint8_t surv_engine_score(void) {
  return s_score;
}

// --- Rastreador de persistencia (de eye-spy, Apache-2.0) --------------------
// Una MAC que no matchea ninguna firma pero reaparece a lo largo del tiempo es
// la senal de un seguidor. Se purga a los 30 min de ausencia.
#define SURV_TRACK_MAX      50
#define SURV_TRACK_MIN_SEEN 3
#define SURV_TRACK_MIN_SPAN 300000u   // 5 min
#define SURV_TRACK_PURGE_MS 1800000u  // 30 min

typedef struct {
  uint8_t  mac[6];
  uint8_t  seen;
  uint32_t first_ms;
  uint32_t last_ms;
  bool     scored;
  bool     used;
} surv_track_t;

static surv_track_t s_track[SURV_TRACK_MAX];

bool surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms) {
  surv_track_t* slot = NULL;
  for (int i = 0; i < SURV_TRACK_MAX; i++) {
    if (s_track[i].used && (now_ms - s_track[i].last_ms) > SURV_TRACK_PURGE_MS) {
      s_track[i].used = false;
    }
    if (s_track[i].used && memcmp(s_track[i].mac, mac, 6) == 0) {
      slot = &s_track[i];
      break;
    }
    if (!s_track[i].used && slot == NULL) {
      slot = &s_track[i];
    }
  }
  if (slot == NULL) {
    return false;
  }
  if (!slot->used) {
    memcpy(slot->mac, mac, 6);
    slot->used = true;
    slot->seen = 0;
    slot->scored = false;
    slot->first_ms = now_ms;
  }
  slot->seen++;
  slot->last_ms = now_ms;
  if (!slot->scored && slot->seen >= SURV_TRACK_MIN_SEEN &&
      (now_ms - slot->first_ms) >= SURV_TRACK_MIN_SPAN) {
    slot->scored = true;
    surv_event_t ev = {0};
    memcpy(ev.mac, mac, 6);
    ev.klass = SURV_CLASS_PERSIST;
    ev.tier = SURV_TIER_SSID;
    ev.rssi = 0;
    ev.proto = SURV_PROTO_BLE;
    surv_engine_submit(&ev, 2, now_ms);
    return true;
  }
  return false;
}
```

`surv_engine_reset()` debe limpiar también `s_track` con
`memset(s_track, 0, sizeof(s_track));`.

- [ ] **Step 4: Ejecutar y verificar que pasan**

Run: `idf.py build && ./build/test_surv.elf -v` en `test_apps/surv`.
Esperado: los ocho tests de `[surv][engine]` pasan.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add scoring engine with decay, cooldown and tier dedupe"
```

---

## Task 8: Parser del overlay de firmas

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_overlay.h`
- Modify: `firmware/components/surveillance_detect/surv_overlay.c`
- Test: `firmware/components/surveillance_detect/test_apps/surv/main/test_surv_overlay.c`

**Interfaces:**
- Consumes: `surv_oui_entry_t`, `surv_class_t`, `surv_kw_entry_t`, `surv_uuid_entry_t`, `surv_ie_tok_t`, `surv_ie_add_signature()`, `surv_ie_reset_signatures()`, `surv_signatures_build_effective*()`.
- Produces:
  ```c
  typedef struct { uint16_t added; uint16_t removed; uint16_t skipped; } surv_overlay_stats_t;
  void surv_overlay_reset(void);
  surv_overlay_stats_t surv_overlay_parse(const char* text);
  ```

El parser recibe **texto en memoria**, no una ruta. Leer el archivo es
responsabilidad de la app (Task 17). Así el parser es 100% testeable en host.

- [ ] **Step 1: Escribir los tests que fallan**

`test_apps/surv/main/test_surv_overlay.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include "surv_overlay.h"
#include "unity.h"

TEST_CASE("un OUI anadido pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  surv_match_init();
  const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("un OUI quitado deja de matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("-oui,70:c9:4e\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.removed);
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("los comentarios y las lineas vacias se ignoran", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("# comentario\n\n   \n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(0, st.removed);
  TEST_ASSERT_EQUAL_UINT16(0, st.skipped);
}

TEST_CASE("una linea malformada se cuenta como saltada", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(
      "+oui,noesunamac,flock,5,2\n"
      "+oui,aa:bb\n"
      "basura\n"
      "+oui,dd:ee:ff,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  TEST_ASSERT_EQUAL_UINT16(3, st.skipped);
}

TEST_CASE("un texto nulo o vacio no rompe", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(NULL);
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  st = surv_overlay_parse("");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
}

TEST_CASE("una keyword de SSID anadida pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+ssid,mikamara,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const surv_kw_entry_t* e = surv_match_ssid("red-MiKaMaRa-2");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
}

TEST_CASE("un UUID anadido pasa a matchear", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+uuid,fd6f,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t adv[] = {0x03, 0x03, 0x6f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(1, surv_match_ble_adv(adv, sizeof(adv), hits));
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, hits[0].klass);
}

TEST_CASE("una firma IE del overlay se suma a la compilada",
          "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,45,191\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0x02, 0x00, 0x00,
                         0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
  surv_overlay_reset();
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("una firma IE malformada se salta", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221:abc\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("se respeta el tope de 256 OUIs", "[surv][overlay]") {
  surv_overlay_reset();
  char buf[64];
  surv_overlay_stats_t st = {0};
  for (int i = 0; i < 300; i++) {
    snprintf(buf, sizeof(buf), "+oui,%02x:%02x:%02x,cam,3,2\n",
             (i >> 16) & 0xff, (i >> 8) & 0xff, i & 0xff);
    surv_overlay_stats_t r = surv_overlay_parse(buf);
    st.added += r.added;
    st.skipped += r.skipped;
  }
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(256, st.added);
  TEST_ASSERT_GREATER_THAN_UINT16(0, st.skipped);
}
```

- [ ] **Step 2: Ejecutar y verificar que falla**

Run: `idf.py build` en `test_apps/surv`. Esperado: `surv_overlay.h` no existe.

- [ ] **Step 3: Reestructurar las tablas para admitir overlay**

`surv_match_init()` deja de leer la tabla base directamente: construye una tabla
efectiva en RAM = base + añadidos − quitados. En `surv_signatures.c` añadir:

```c
#define SURV_OVERLAY_MAX_OUIS 256

static surv_oui_entry_t s_effective[74 + SURV_OVERLAY_MAX_OUIS];
static uint16_t         s_effective_count;

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
}
```

`surv_signatures_ouis()` y `surv_signatures_oui_count()` devuelven la tabla
efectiva si se construyó, y la base si no. El test de Task 1 (74 OUIs) sigue
pasando porque sin overlay la efectiva es idéntica a la base.

- [ ] **Step 4: Implementar el parser**

`surv_overlay.c` — parsea línea a línea con `strtok_r` sobre una copia acotada,
sin `malloc`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_overlay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surv_signatures.h"

#define MAX_ADD    256
#define MAX_REMOVE 64
#define MAX_ADD_KW   64
#define MAX_ADD_UUID 16
#define MAX_LINE     128
#define KW_MAX_LEN   24

static surv_oui_entry_t s_add[MAX_ADD];
static uint16_t         s_add_count;
static uint8_t          s_remove[MAX_REMOVE][3];
static uint16_t         s_remove_count;
static surv_kw_entry_t   s_add_kw[MAX_ADD_KW];
static char              s_kw_pool[MAX_ADD_KW][KW_MAX_LEN];
static uint16_t          s_add_kw_count;
static surv_uuid_entry_t s_add_uuid[MAX_ADD_UUID];
static uint16_t          s_add_uuid_count;

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
  const char* colon = strchr(s, ':');
  int         tag = atoi(s);
  if (tag < 0 || tag > 255) {
    return false;
  }
  out->tag = (uint8_t) tag;
  if (colon == NULL) {
    return true;
  }
  const char* hex = colon + 1;
  size_t      hl = strlen(hex);
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
  if (strcmp(s, "flock") == 0) return SURV_CLASS_FLOCK;
  if (strcmp(s, "alpr") == 0) return SURV_CLASS_ALPR;
  if (strcmp(s, "cam") == 0) return SURV_CLASS_CAM;
  if (strcmp(s, "axon") == 0) return SURV_CLASS_AXON;
  if (strcmp(s, "glasses") == 0) return SURV_CLASS_GLASSES;
  return SURV_CLASS_NONE;
}

static bool handle_line(char* line, surv_overlay_stats_t* st) {
  while (*line == ' ' || *line == '\t') line++;
  if (*line == '\0' || *line == '#' || *line == '\r') {
    return true;  // ignorada, no cuenta como saltada
  }
  char  sign = *line++;
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
    uint8_t       n = 0;
    const char*   tok = value;
    while (tok != NULL && n < SURV_IE_MAX_TOKS) {
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
```

`surv_signatures_build_effective_kws()` y `surv_signatures_build_effective_uuids()`
son los gemelos de `surv_signatures_build_effective()` para keywords y UUIDs:
copian la tabla base a un arreglo estático en RAM y añaden las del overlay al
final. Sin overlay devuelven la base, así que los tests de las Tasks 3 y 4
siguen pasando.

- [ ] **Step 5: Ejecutar y verificar que pasan**

Run: `idf.py build && ./build/test_surv.elf -v` en `test_apps/surv`.
Esperado: los seis tests de `[surv][overlay]` pasan y los de Task 2 siguen
pasando (la tabla efectiva sin overlay es idéntica a la base).

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add microSD signature overlay parser"
```

---

## Task 9: Ring buffer, callbacks de radio y API pública

**Files:**
- Create: `firmware/components/surveillance_detect/include/surveillance_detect.h`
- Modify: `firmware/components/surveillance_detect/surveillance_detect.c`
- Modify: `firmware/components/radio_selector/include/radio_selector.h`, `radio_selector.c`

**Interfaces:**
- Consumes: `surv_match_*`, `surv_ie_*`, `surv_engine_*`.
- Produces:
  ```c
  typedef enum { SURV_PROFILE_FLOCK = 0, SURV_PROFILE_SURVEIL, SURV_PROFILE_TRACKERS } surv_profile_t;
  typedef void (*surv_detect_cb_t)(const surv_event_t* ev, uint8_t score);
  esp_err_t surv_begin(surv_profile_t profile, bool active_scan);
  void      surv_stop(void);
  void      surv_register_cb(surv_detect_cb_t cb);
  uint32_t  surv_queue_overflows(void);
  ```

- [ ] **Step 1: Añadir `RADIO_SELECT_SURVEILLANCE`**

En `radio_selector.h`, al enum:

```c
typedef enum {
  RADIO_SELECT_ZIGBEE_SWITCH,
  RADIO_SELECT_ZIGBEE_SNIFFER,
  RADIO_SELECT_THREAD,
  RADIO_SELECT_ZIGBEE_LIGHT,
  RADIO_SELECT_SURVEILLANCE,
} radio_select_options_t;

void radio_selector_set_surveillance(void);
```

En `radio_selector.c`:

```c
void radio_selector_set_surveillance(void) {
  radio_selected_option = RADIO_SELECT_SURVEILLANCE;
}
```

- [ ] **Step 2: Implementar el ring buffer y el callback promiscuo**

`surveillance_detect.c`. El callback solo compara y encola; nada de `printf`,
`malloc` ni escritura a disco:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surveillance_detect.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "surv_engine.h"
#include "surv_ie.h"
#include "surv_match.h"

#define SURV_QUEUE_LEN 32

typedef struct {
  surv_event_t ev;
  uint8_t      points;
} surv_qitem_t;

static surv_qitem_t     s_queue[SURV_QUEUE_LEN];
static volatile uint8_t s_q_head, s_q_tail;
static volatile uint32_t s_overflows;
static portMUX_TYPE     s_q_mux = portMUX_INITIALIZER_UNLOCKED;

static bool queue_push(const surv_event_t* ev, uint8_t points) {
  bool ok = false;
  portENTER_CRITICAL_ISR(&s_q_mux);
  uint8_t next = (uint8_t) ((s_q_head + 1) % SURV_QUEUE_LEN);
  if (next != s_q_tail) {
    s_queue[s_q_head].ev = *ev;
    s_queue[s_q_head].points = points;
    s_q_head = next;
    ok = true;
  } else {
    s_overflows++;
  }
  portEXIT_CRITICAL_ISR(&s_q_mux);
  return ok;
}

uint32_t surv_queue_overflows(void) {
  return s_overflows;
}

static void IRAM_ATTR wifi_sniffer_cb(void* buf,
                                      wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*) buf;
  if (p->rx_ctrl.rssi < SURV_RSSI_MIN) {
    return;
  }
  const uint8_t* pl = p->payload;
  int            len = p->rx_ctrl.sig_len;
  if (len < 24) {
    return;
  }
  const uint8_t* addr1 = pl + 4;
  const uint8_t* addr2 = pl + 10;
  const uint8_t* addr3 = pl + 16;
  uint8_t        subtype = (uint8_t) ((pl[0] >> 4) & 0x0f);
  bool           is_mgmt = ((pl[0] & 0x0c) == 0x00);

  surv_event_t ev = {0};
  ev.rssi = p->rx_ctrl.rssi;
  ev.channel = p->rx_ctrl.channel;
  ev.proto = SURV_PROTO_WIFI;

  // Tier 3/4: probe request con SSID wildcard desde un OUI conocido.
  if (is_mgmt && subtype == 4 && len > 24) {
    const surv_oui_entry_t* e = surv_match_oui(addr2);
    if (e != NULL) {
      const uint8_t* ies = pl + 24;
      int            ielen = len - 24;
      if (surv_ie_is_wildcard_probe(ies, ielen) == 1) {
        memcpy(ev.mac, addr2, 6);
        ev.klass = e->klass;
        ev.tier = surv_ie_matches_flock(ies, ielen) ? SURV_TIER_IE_SIG
                                                    : SURV_TIER_PROBE;
        if (ev.tier > e->tier) {
          ev.tier = e->tier;  // el techo de la firma manda
        }
        queue_push(&ev, e->points);
        return;
      }
    }
  }

  // Drone Remote ID por WiFi: trama de gestion dirigida a la MAC NaN fija de
  // ASTM F3411. No depende de ningun OUI. Motor de eye-spy (Apache-2.0).
  static const uint8_t ODID_NAN[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
  if (is_mgmt && memcmp(addr1, ODID_NAN, 6) == 0) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = SURV_CLASS_ODID;
    ev.tier = SURV_TIER_ADDR2;
    queue_push(&ev, 4);
    return;
  }

  // Tier 2: OUI en addr2 de cualquier trama.
  const surv_oui_entry_t* e2 = surv_match_oui(addr2);
  if (e2 != NULL) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = e2->klass;
    ev.tier = SURV_TIER_ADDR2 > e2->tier ? e2->tier : SURV_TIER_ADDR2;
    queue_push(&ev, e2->points);
    return;
  }

  // Tier 1: OUI en addr1 o addr3. Ecos, propensos a falso positivo.
  const surv_oui_entry_t* e1 = surv_match_oui(addr1);
  if (e1 == NULL && is_mgmt) {
    e1 = surv_match_oui(addr3);
    addr1 = addr3;
  }
  if (e1 != NULL) {
    memcpy(ev.mac, addr1, 6);
    ev.klass = e1->klass;
    ev.tier = SURV_TIER_ADDR13 > e1->tier ? e1->tier : SURV_TIER_ADDR13;
    queue_push(&ev, e1->points);
  }
}
```

- [ ] **Step 3: Implementar la task del motor**

```c
static TaskHandle_t     s_engine_task;
static volatile bool    s_running;
static surv_detect_cb_t s_user_cb;

static void engine_emit(const surv_event_t* ev, uint8_t score) {
  if (s_user_cb != NULL) {
    s_user_cb(ev, score);
  }
}

static void engine_task(void* arg) {
  (void) arg;
  while (s_running) {
    uint32_t now = (uint32_t) (esp_timer_get_time() / 1000);
    while (s_q_tail != s_q_head) {
      surv_qitem_t item;
      portENTER_CRITICAL(&s_q_mux);
      item = s_queue[s_q_tail];
      s_q_tail = (uint8_t) ((s_q_tail + 1) % SURV_QUEUE_LEN);
      portEXIT_CRITICAL(&s_q_mux);
      surv_engine_submit(&item.ev, item.points, now);
    }
    surv_engine_tick(now);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  s_engine_task = NULL;
  vTaskDelete(NULL);
}
```

`surv_begin()` llama a `surv_match_init()`, `surv_engine_reset()`,
`surv_engine_register_emit_cb(engine_emit)`, `radio_selector_set_surveillance()`,
crea la task con `xTaskCreate(engine_task, "surv_engine", 4096, NULL, 5, &s_engine_task)`
y arranca `surv_radio_start(profile, active_scan)` (Task 10).

- [ ] **Step 4: Compilar para el target real**

```bash
cd firmware && idf.py build
```

Esperado: build OK.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect firmware/components/radio_selector
git commit -m "feat(surveillance): add lock-free queue, sniffer callback and public API"
```

---

## Task 10: Planificador de radio

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_radio.c`
- Create: `firmware/components/surveillance_detect/include/surv_radio.h`

**Interfaces:**
- Consumes: `surv_profile_t`, `wifi_sniffer_cb`, `surv_match_ble_adv()`.
- Produces: `esp_err_t surv_radio_start(surv_profile_t p, bool active_scan)`, `void surv_radio_stop(void)`, `uint8_t surv_radio_current_channel(void)`.

- [ ] **Step 1: Implementar el salto de canal con recorte por región**

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_radio.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SURV_DWELL_MS 250

static const uint8_t HOP_PRIMARY[]  = {11, 6, 1};
static const uint8_t HOP_EXTENDED[] = {13, 8, 3};

// Recorta al nchan de la region configurada: con GLOBAL (default de Minino) los
// canales son 1-11 y saltar al 13 no escucha nada.
static bool channel_allowed(uint8_t ch) {
  wifi_country_t c;
  if (esp_wifi_get_country(&c) != ESP_OK) {
    return ch <= 11;
  }
  return ch >= c.schan && ch < (uint8_t) (c.schan + c.nchan);
}
```

La task de hopping recorre `HOP_PRIMARY` con dwell de 250 ms y, en perfil
`SURV_PROFILE_SURVEIL`, intercala `HOP_EXTENDED` cada cuarta vuelta, saltando
los canales que `channel_allowed()` rechace.

- [ ] **Step 2: Implementar las fases**

Ambos stacks quedan inicializados; lo exclusivo es el escaneo. **No** se hace
`esp_bluedroid_deinit` por ciclo: fragmenta el heap y cuesta cientos de ms.

```c
static void radio_task(void* arg) {
  while (s_running) {
    switch (s_profile) {
      case SURV_PROFILE_FLOCK:
        wifi_window_ms(20000);
        break;
      case SURV_PROFILE_SURVEIL:
        ble_window_ms(6000);
        wifi_window_ms(14000);
        break;
      case SURV_PROFILE_TRACKERS:
        ble_window_ms(20000);
        break;
    }
    if (s_active_scan && s_profile != SURV_PROFILE_TRACKERS) {
      active_scan_window_ms(3000);
    }
  }
  vTaskDelete(NULL);
}
```

`ble_window_ms()` llama a `esp_ble_gap_start_scanning()` con
`BLE_SCAN_TYPE_PASSIVE` y `window == interval`; al terminar,
`esp_ble_gap_stop_scanning()`. `wifi_window_ms()` hace
`esp_wifi_set_promiscuous(true)` con `wifi_sniffer_cb`, corre el hopping y
termina con `esp_wifi_set_promiscuous(false)`.

- [ ] **Step 3: Compilar y verificar en hardware**

```bash
cd firmware && idf.py build && idf.py -p /dev/ttyACM0 flash monitor
```

Sin app todavía; se invoca desde el monitor por consola o con una llamada
temporal en `app_main`. Esperado en el log: alternancia de fases y cambio de
canal cada 250 ms, sin `Guru Meditation` ni watchdog en 10 minutos.

- [ ] **Step 4: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add radio phase scheduler with region-aware hopping"
```

---

## Task 11: App, entrada de menú y pantallas

**Files:**
- Create: `firmware/main/apps/surveillance/surveillance_module.c`, `.h`
- Create: `firmware/main/apps/surveillance/surveillance_screens.c`, `.h`
- Modify: `firmware/main/modules/menus_module/menus_include/menus.h`

**Interfaces:**
- Consumes: `surv_begin()`, `surv_stop()`, `surv_register_cb()`, `surv_queue_overflows()`.
- Produces: `void surveillance_module_begin(void)` — la usa `menus.h` como `on_enter_cb`.

- [ ] **Step 1: Añadir la entrada de menú**

En `menus.h`, al enum, junto a `MENU_GPIO_APPS`:

```c
  MENU_SURVEILLANCE,
```

y a la tabla de menús, con `parent_idx = MENU_APPLICATIONS`:

```c
    {.display_name = "Surveillance",
     .menu_idx = MENU_SURVEILLANCE,
     .parent_idx = MENU_APPLICATIONS,
     .entry_cmd = "surveillance",
     .last_selected_submenu = 0,
     .on_enter_cb = surveillance_module_begin,
     .on_exit_cb = NULL,
     .is_visible = true},
```

más `#include "surveillance_module.h"` en la zona de includes de `menus.h`.

- [ ] **Step 2: Implementar la pantalla principal**

`surveillance_screens.c`, con `oled_screen_display_text` sobre las 8 páginas:

```c
void surveillance_screens_show_status(uint8_t score, const char* profile,
                                      const char* last_label, uint8_t last_tier,
                                      int8_t rssi, uint8_t channel) {
  char buf[24];
  oled_screen_clear_buffer();
  snprintf(buf, sizeof(buf), "SURVEIL [%s]", profile);
  oled_screen_display_text(buf, 0, 0, false);

  const char* level = score >= 6 ? "ALERT" : (score >= 3 ? "CAUTION" : "CLEAR");
  snprintf(buf, sizeof(buf), "score %2d  %s", score, level);
  oled_screen_display_text(buf, 0, 1, false);

  if (last_label != NULL) {
    snprintf(buf, sizeof(buf), "%s  T%d", last_label, last_tier);
    oled_screen_display_text(buf, 0, 2, false);
    snprintf(buf, sizeof(buf), "%ddBm  ch %d", rssi, channel);
    oled_screen_display_text(buf, 0, 3, false);
  }
  oled_screen_display_show();
}
```

- [ ] **Step 3: Implementar la pantalla de ayuda con el aviso regional**

El spec §9.4 lo exige: con la tabla base, en México la app puede correr una hora
sin una sola detección, y ese es el resultado correcto. Sin decirlo, parece
firmware roto.

```c
static const char* HELP_LINES[] = {
    "Detector pasivo de",
    "vigilancia.",
    "",
    "Flock/ALPR se usa",
    "en EUA y Canada.",
    "En MX es normal",
    "no ver nada.",
    "",
    "Carga firmas de tu",
    "region en microSD:",
    "surveil/",
    "signatures.csv",
};

void surveillance_screens_show_help(uint8_t page) {
  oled_screen_clear();
  uint8_t rows = oled_screen_get_pages();
  for (uint8_t i = 0; i < rows && (page * rows + i) < 12; i++) {
    oled_screen_display_text((char*) HELP_LINES[page * rows + i], 0, i, false);
  }
  oled_screen_display_show();
}
```

Se llega con BUTTON_UP desde la pantalla principal; BUTTON_LEFT vuelve.

- [ ] **Step 4: Implementar el ciclo de vida y los botones**

`surveillance_module.c`, siguiendo el patrón de
`main/apps/wifi/deauth_detector/detector.c`:

Helpers que usa el bloque siguiente, en el mismo archivo:

```c
static surv_profile_t load_profile_from_prefs(void) {
  uint8_t v = preferences_get_uchar("surv_profile", SURV_PROFILE_SURVEIL);
  return (v <= SURV_PROFILE_TRACKERS) ? (surv_profile_t) v
                                      : SURV_PROFILE_SURVEIL;
}

static bool load_active_scan_from_prefs(void) {
  return preferences_get_uchar("surv_active", 0) != 0;
}

// Llamado desde la task del motor. Solo guarda estado y marca la pantalla como
// sucia: el refresco del OLED y la escritura a microSD ocurren en el bucle de
// la app, nunca aqui.
static void on_detection(const surv_event_t* ev, uint8_t score) {
  s_last_event = *ev;
  s_last_score = score;
  s_have_event = true;
  s_screen_dirty = true;
}
```

`surveillance_screens_show_radio_busy()` es una pantalla estática de
`surveillance_screens.c` con el texto `802.15.4 activo` / `reinicia Minino`.

```c
void surveillance_module_begin(void) {
  if (radio_selector_is_stack_initialized()) {
    surveillance_screens_show_radio_busy();
    menus_module_set_app_state(true, surveillance_input_cb);
    return;  // 802.15.4 tiene la antena; no se disputa
  }
  menus_module_set_app_state(true, surveillance_input_cb);
  surv_register_cb(on_detection);
  surv_begin(load_profile_from_prefs(), load_active_scan_from_prefs());
}

static void surveillance_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name == BUTTON_LEFT) {
    surveillance_module_stop();
  }
}

void surveillance_module_stop(void) {
  surv_stop();
  oled_screen_clear();
  menus_module_restart();
}
```

- [ ] **Step 5: Compilar, flashear y verificar en hardware**

```bash
cd firmware && idf.py build && idf.py -p /dev/ttyACM0 flash monitor
```

1. Menú → Applications → Surveillance: la app arranca y muestra `score 0 CLEAR`.
2. BUTTON_UP muestra la ayuda con el aviso regional; BUTTON_LEFT vuelve.
3. BUTTON_LEFT desde la principal devuelve al menú sin colgarse.
4. Reentrar y volver a salir tres veces: no debe caer el heap libre de forma
   monotónica (comprobar con `esp_get_free_heap_size()` en el log).

- [ ] **Step 6: Commit**

```bash
git add firmware/main/apps/surveillance firmware/main/modules/menus_module
git commit -m "feat(surveillance): add app, menu entry and status screen"
```

---

## Task 12: Buzzer por tier y LEDs por cadencia

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`

**Interfaces:**
- Consumes: `buzzer_set_freq()`, `buzzer_play_for()`, `led_start_blink()`, `led_start_breath()`, `led_stop()`, `preferences_get_uchar()`.
- Produces: `void surveillance_alert(uint8_t tier, uint8_t score)`.

Minino no tiene LED RGB —`leds.h` expone `LED_LEFT`/`LED_RIGHT` con brillo, sin
color—, así que el semáforo va en pantalla y los LEDs codifican el nivel por
cadencia.

- [ ] **Step 1: Implementar los tonos por tier**

```c
// Tonos de flock-you (MIT): el metodo de deteccion se distingue de oido
// mientras se maneja, sin mirar la pantalla.
static void tier_chirp(uint8_t tier) {
  uint8_t mask = preferences_get_uchar("surv_beep", 0x1F);
  if ((mask & (1u << tier)) == 0) {
    return;  // silenciado: la deteccion se registra igual
  }
  switch (tier) {
    case SURV_TIER_IE_SIG:
      buzzer_set_freq(2000); buzzer_play_for(60);
      buzzer_set_freq(2800); buzzer_play_for(60);
      break;
    case SURV_TIER_PROBE:
      buzzer_set_freq(1400); buzzer_play_for(60);
      buzzer_set_freq(1800); buzzer_play_for(60);
      break;
    case SURV_TIER_ADDR2:  buzzer_set_freq(1200); buzzer_play_for(80); break;
    case SURV_TIER_ADDR13: buzzer_set_freq(800);  buzzer_play_for(80); break;
    default:               buzzer_set_freq(600);  buzzer_play_for(80); break;
  }
}
```

- [ ] **Step 2: Implementar el semáforo en LEDs**

```c
static void update_leds(uint8_t score) {
  static uint8_t last_level = 0xff;
  uint8_t level = score >= 6 ? 2 : (score >= 3 ? 1 : 0);
  if (level == last_level) {
    return;
  }
  last_level = level;
  led_stop(LED_LEFT);
  led_stop(LED_RIGHT);
  if (level == 1) {
    led_start_breath(LED_LEFT, 2000);
  } else if (level == 2) {
    // led_start_blink(led, duty, pulse_count, time_on, time_off, time_out)
    // pulse_count 0 y time_out 0 = parpadeo indefinido hasta led_stop().
    led_start_blink(LED_LEFT, 100, 0, 200, 200, 0);
    led_start_blink(LED_RIGHT, 100, 0, 200, 200, 0);
  }
}
```

La firma de `led_start_blink` es la de
`firmware/components/leds/include/leds.h`: seis parámetros, no dos. Confirmar el
significado de `duty` y `time_out` en `leds.c` antes de dar por buenos los
valores: si `pulse_count = 0` no significa "indefinido", usar un valor alto y
rearmar el parpadeo desde `update_leds()`.

- [ ] **Step 3: Verificar en hardware**

Flashear y, con el overlay de microSD apuntando a un dispositivo propio
(Task 17) o forzando una detección de prueba:

1. Tier 4 suena distinto de tier 1 y se distingue a oído.
2. Con `surv_beep` = 0 en NVS no suena nada pero la detección sigue registrándose.
3. Los LEDs pasan de apagado a respiración a parpadeo según sube el score.

- [ ] **Step 4: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): add per-tier buzzer tones and LED cadence levels"
```

---

## Task 13: Registro CSV con GPS

**Files:**
- Create: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`

**Interfaces:**
- Consumes: `sd_card_append_to_file()`, `gps_module_get_instance()`, `get_full_date_time()`, `surv_event_t`.
- Produces: `void surveillance_log_begin(void)`, `void surveillance_log_detection(const surv_event_t* ev, uint8_t score, gps_t* gps)`, `void surveillance_log_flush(void)`.

- [ ] **Step 1: Definir el formato**

En `surveillance_log.h`. Las 14 columnas de WigleWifi primero, las cuatro propias
al final, para que `cut -d, -f1-14` produzca un archivo subible a WiGLE:

```c
#define SURV_DIR_NAME    "surveil"
#define SURV_CSV_LINE    200   // no caben en los 150 de CSV_LINE_SIZE
#define SURV_CSV_LINES   200
#define SURV_CSV_BUF_SZ  (SURV_CSV_LINE * SURV_CSV_LINES)

#define SURV_CSV_HEADER                                                  \
  FORMAT_VERSION ",appRelease=" APP_VERSION ",model=" MODEL              \
  ",release=" RELEASE ",device=" DEVICE ",display=" DISPLAY              \
  ",board=" BOARD ",brand=" BRAND ",star=" STAR ",body=" BODY            \
  ",subBody=" SUB_BODY "\n"                                              \
  "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"  \
  "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type,"    \
  "Class,Tier,Method,Score"
```

- [ ] **Step 2: Escribir los helpers de formato**

Todos en `surveillance_log.c`, con buffers estáticos: no se asigna memoria por
detección.

```c
static const char* class_name(surv_class_t k) {
  switch (k) {
    case SURV_CLASS_FLOCK:         return "FLOCK";
    case SURV_CLASS_FLOCK_MFR:     return "FLOCK_MFR";
    case SURV_CLASS_ALPR:          return "ALPR";
    case SURV_CLASS_SOUNDTHINKING: return "SOUNDTHINKING";
    case SURV_CLASS_AXON:          return "AXON";
    case SURV_CLASS_GLASSES:       return "GLASSES";
    case SURV_CLASS_CAM:           return "CAM";
    case SURV_CLASS_AIRTAG:        return "AIRTAG";
    case SURV_CLASS_SMARTTAG:      return "SMARTTAG";
    case SURV_CLASS_TILE:          return "TILE";
    case SURV_CLASS_APPLE_NEARBY:  return "APPLE_NEARBY";
    case SURV_CLASS_IBEACON:       return "IBEACON";
    case SURV_CLASS_ODID:          return "ODID";
    case SURV_CLASS_SKIMMER:       return "SKIMMER";
    case SURV_CLASS_MESHCORE:      return "MESHCORE";
    case SURV_CLASS_RAVEN:         return "RAVEN";
    case SURV_CLASS_PERSIST:       return "PERSIST";
    default:                       return "UNKNOWN";
  }
}

static const char* method_name(uint8_t tier) {
  switch (tier) {
    case SURV_TIER_IE_SIG: return "wildcard_probe_ie_sig";
    case SURV_TIER_PROBE:  return "wildcard_probe";
    case SURV_TIER_ADDR2:  return "oui_addr2";
    case SURV_TIER_ADDR13: return "oui_addr1_addr3";
    default:               return "ssid_kw";
  }
}

static const char* mac_str(const uint8_t mac[6]) {
  static char buf[18];
  snprintf(buf, sizeof(buf), MAC_ADDRESS_FORMAT, mac[0], mac[1], mac[2], mac[3],
           mac[4], mac[5]);
  return buf;
}

// Formato de fecha de wardriving. Sin fix GPS no hay reloj fiable: se escribe
// el uptime en ms, que al menos ordena las detecciones entre si.
static const char* date_str(gps_t* gps) {
  static char buf[32];
  if (gps != NULL && gps->valid) {
    char* full = get_full_date_time(gps);
    snprintf(buf, sizeof(buf), "%s", full);
    free(full);
  } else {
    snprintf(buf, sizeof(buf), "uptime_%llu",
             (unsigned long long) (esp_timer_get_time() / 1000));
  }
  return buf;
}

static uint16_t freq_of(uint8_t channel) {
  if (channel == 0) {
    return 0;  // BLE
  }
  return (uint16_t) (channel == 14 ? 2484 : 2407 + channel * 5);
}

// Acumula en RAM y vuelca por lotes. Escribir por deteccion desgastaria la
// microSD y bloquearia el drenaje del ring buffer.
static char     s_buf[SURV_CSV_BUF_SZ];
static uint16_t s_lines;

static void append_buffered(const char* line) {
  if (strlen(s_buf) + strlen(line) >= sizeof(s_buf) ||
      s_lines >= SURV_CSV_LINES) {
    sd_card_append_to_file(s_csv_name, s_buf);
    s_buf[0] = '\0';
    s_lines = 0;
  }
  strcat(s_buf, line);
  s_lines++;
}
```

`surveillance_log_flush()` hace el último `sd_card_append_to_file(s_csv_name, s_buf)`
al salir de la app.

- [ ] **Step 3: Implementar la escritura sin fix GPS**

```c
void surveillance_log_detection(const surv_event_t* ev, uint8_t score,
                                gps_t* gps) {
  char line[SURV_CSV_LINE];
  char lat[16] = "";
  char lon[16] = "";
  // Sin fix se escribe igual con coordenada vacia: perder una camara
  // confirmada por no tener GPS seria el peor intercambio posible.
  if (gps != NULL && gps->valid) {
    snprintf(lat, sizeof(lat), "%f", gps->latitude);
    snprintf(lon, sizeof(lon), "%f", gps->longitude);
  }
  snprintf(line, sizeof(line),
           "%s,,,%s,%d,%u,%d,%s,%s,%f,%f,,,%s,%s,%d,%s,%d\n",
           mac_str(ev->mac), date_str(gps), ev->channel,
           freq_of(ev->channel), ev->rssi, lat, lon,
           gps ? gps->altitude : 0.0, (double) GPS_ACCURACY,
           ev->proto == SURV_PROTO_WIFI ? "WIFI" : "BLE",
           class_name(ev->klass), ev->tier, method_name(ev->tier), score);
  append_buffered(line);
}
```

- [ ] **Step 4: Verificar en hardware**

1. Con microSD y GPS con fix: recorrer una manzana con el overlay apuntando a un
   dispositivo propio.
2. Sacar la tarjeta y comprobar que `surveil/*.csv` tiene el header correcto y
   filas con coordenadas.
3. `cut -d, -f1-14 surveil/xxx.csv > wigle.csv` y comprobar que las columnas
   cuadran con un CSV de `warfi/`.
4. Repetir sin fix GPS: las filas deben aparecer con lat/lon vacías, no faltar.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): log detections to WiGLE-compatible CSV with GPS"
```

---

## Task 14: Exportación GPX de tier 4

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`

**Interfaces:**
- Consumes: lo de Task 13.
- Produces: `void surveillance_log_gpx_waypoint(const surv_event_t* ev, gps_t* gps)`.

DeFlock **no** consume CSV: es OpenStreetMap con nodos etiquetados. El GPX se
carga como capa en el editor iD para colocar los nodos sin repetir el recorrido.

- [ ] **Step 1: Implementar el waypoint**

Solo para tier 4 con coordenada válida:

```c
void surveillance_log_gpx_waypoint(const surv_event_t* ev, gps_t* gps) {
  if (ev->tier != SURV_TIER_IE_SIG || gps == NULL || !gps->valid) {
    return;
  }
  char wpt[200];
  snprintf(wpt, sizeof(wpt),
           "<wpt lat=\"%f\" lon=\"%f\"><name>%s</name>"
           "<desc>tier4 %s ch%d %ddBm</desc></wpt>\n",
           gps->latitude, gps->longitude, mac_str(ev->mac),
           class_name(ev->klass), ev->channel, ev->rssi);
  sd_card_append_to_file(s_gpx_name, wpt);
}
```

El archivo se abre con la cabecera GPX en `surveillance_log_begin()` y se cierra
con `</gpx>` en `surveillance_log_flush()`.

- [ ] **Step 2: Verificar**

Generar un GPX con al menos un waypoint y abrirlo en
[openstreetmap.org/edit](https://www.openstreetmap.org/edit) como capa GPX.
Esperado: el waypoint aparece en la posición correcta.

- [ ] **Step 3: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): export tier-4 detections as GPX waypoints"
```

---

## Task 15: Evidencia en pcap

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`
- Modify: `firmware/components/surveillance_detect/surveillance_detect.c`, `include/surveillance_detect.h`

**Interfaces:**
- Consumes: `espressif__pcap` (ya en `managed_components`, usado por `wifi_sniffer`).
- Produces: el evento encolado lleva una copia de la trama cuando `tier >= 3`.

Ni flock-you ni eye-spy guardan la evidencia. Esto convierte a Minino en
herramienta de investigación: la firma IE se verifica a mano en Wireshark en
lugar de creerle al firmware. Los mismos volcados alimentan los vectores
pendientes de la Task 6.

- [ ] **Step 1: Ampliar el item de cola para tier 3 y 4**

```c
#define SURV_EVIDENCE_MAX 320

typedef struct {
  surv_event_t ev;
  uint8_t      points;
  uint16_t     raw_len;                    // 0 si no se copio
  uint8_t      raw[SURV_EVIDENCE_MAX];
} surv_qitem_t;
```

La copia solo ocurre cuando el match ya disparó con `tier >= SURV_TIER_PROBE`,
así que el coste no está en el camino de la trama común.

- [ ] **Step 2: Abrir la sesión de pcap**

```c
static pcap_file_handle_t s_pcap;
static FILE*              s_pcap_fp;

esp_err_t surveillance_log_pcap_begin(const char* path) {
  s_pcap_fp = fopen(path, "wb");
  if (s_pcap_fp == NULL) {
    return ESP_FAIL;
  }
  pcap_config_t cfg = {.fp = s_pcap_fp, .major_version = PCAP_DEFAULT_VERSION_MAJOR,
                       .minor_version = PCAP_DEFAULT_VERSION_MINOR,
                       .time_zone = PCAP_DEFAULT_TIME_ZONE};
  ESP_RETURN_ON_ERROR(pcap_new_session(&cfg, &s_pcap), TAG, "pcap session");
  // 802.11 sin cabecera de radio: es lo que entrega el modo promiscuo.
  return pcap_write_header(s_pcap, PCAP_LINK_TYPE_802_11);
}
```

Comparar con `firmware/components/wifi_sniffer/cmd_pcap.c`, que ya hace esta
secuencia, y reutilizar sus nombres de campo si difieren de los de arriba.

- [ ] **Step 3: Escribir el pcap con tope de 4 MB**

```c
#define SURV_PCAP_MAX_BYTES (4u * 1024u * 1024u)

static uint32_t s_pcap_bytes;
static bool     s_pcap_full;

void surveillance_log_evidence(const uint8_t* raw, uint16_t len,
                               uint8_t channel) {
  if (s_pcap_full || raw == NULL || len == 0) {
    return;
  }
  if (s_pcap_bytes + len + 16 > SURV_PCAP_MAX_BYTES) {
    s_pcap_full = true;
    // Se deja de escribir evidencia, pero la deteccion y el CSV siguen.
    surveillance_screens_show_pcap_full();
    return;
  }
  int64_t us = esp_timer_get_time();
  pcap_capture_packet(s_pcap, (void*) raw, len, (uint32_t) (us / 1000000),
                      (uint32_t) (us % 1000000));
  s_pcap_bytes += len + 16;
}
```

Al salir de la app: `pcap_del_session(s_pcap)` y `fclose(s_pcap_fp)`, en ese
orden.

- [ ] **Step 4: Verificar en hardware**

1. Provocar una detección tier 3 o 4 con la segunda placa (Task 17).
2. Abrir `surveil/evidence_*.pcap` en Wireshark.
3. Comprobar que la trama es un Probe Request, que el SSID IE tiene longitud 0 y
   que los vendor IE contienen `50:6f:9a:16:03:01:03`.
4. Extraer esos IE y añadirlos como `FLOCK_IES_REAL` al test de la Task 6.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance firmware/components/surveillance_detect
git commit -m "feat(surveillance): dump tier-3/4 frames to pcap evidence file"
```

---

## Task 16: Settings — perfil, scan activo y máscara de buzzer

**Files:**
- Modify: `firmware/main/modules/settings/` (submenú nuevo)
- Modify: `firmware/main/modules/menus_module/menus_include/menus.h`

**Interfaces:**
- Consumes: `preferences_put_uchar()`, `preferences_get_uchar()`.
- Produces: claves NVS `surv_profile` (0–2), `surv_active` (0/1), `surv_beep` (máscara de 5 bits, default `0x1F`).

- [ ] **Step 1: Añadir las tres opciones**

Siguiendo el patrón de `main/modules/settings/wifi/`, con
`general_radio_selection` para el perfil:

- **Perfil**: `Flock/ALPR`, `Vigilancia`, `Trackers`. Default `Vigilancia`.
- **Scan activo**: on/off, default off. Solo visible en los perfiles
  `Flock/ALPR` y `Vigilancia`; en `Trackers` la opción se ignora.
- **Buzzer por tier**: cinco toggles, uno por tier, default todos activos.

- [ ] **Step 2: Mostrar el aviso de scan activo**

Cuando `surv_active` está en 1, la pantalla principal muestra `!` junto al
nombre del perfil: habilitarlo hace que el dispositivo transmita probe requests
y deje de ser indetectable, y eso tiene que verse sin entrar a settings.

- [ ] **Step 3: Verificar la persistencia**

1. Cambiar perfil a `Flock/ALPR`, salir, reiniciar la placa.
2. Entrar a la app: debe arrancar en `Flock/ALPR`.
3. Silenciar tier 1, reiniciar, comprobar que sigue silenciado y que tier 4 suena.

- [ ] **Step 4: Commit**

```bash
git add firmware/main/modules/settings firmware/main/modules/menus_module
git commit -m "feat(surveillance): add radio profile, active scan and beep mask settings"
```

---

## Task 17: Carga del overlay desde microSD y validación de campo

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`
- Create: `docs/surveillance-signatures.md`

**Interfaces:**
- Consumes: `sd_card_read_file()`, `surv_overlay_parse()`, `surv_overlay_reset()`.
- Produces: nada nuevo; cierra el ciclo.

- [ ] **Step 1: Leer el archivo en `surveillance_module_begin()`**

Antes de `surv_begin()`, nunca desde un callback:

```c
static void load_overlay(void) {
  surv_overlay_reset();
  if (sd_card_is_not_mounted() && sd_card_mount() != ESP_OK) {
    s_overlay_status = SURV_OVERLAY_NO_SD;
    return;
  }
  static char text[8192];
  if (read_file_into(SURV_DIR_NAME "/signatures.csv", text, sizeof(text)) !=
      ESP_OK) {
    s_overlay_status = SURV_OVERLAY_NO_FILE;
    return;
  }
  surv_overlay_stats_t st = surv_overlay_parse(text);
  s_overlay_added = st.added;
  s_overlay_status = SURV_OVERLAY_OK;
}
```

La pantalla de arranque muestra `sigs: 74 base +12 SD` o `SD: none`.

- [ ] **Step 2: Escribir la documentación de usuario**

`docs/surveillance-signatures.md` con el formato del archivo (los cinco tipos:
`oui`, `ssid`, `blename`, `uuid`, `iesig`), un ejemplo completo, los topes
(256 OUIs, 64 keywords, 16 UUIDs, 8 firmas IE) y **el aviso regional**, el
mismo que muestra la pantalla de ayuda de la Task 11: Flock Safety se despliega
en Estados Unidos y, en menor medida, Canadá; con la tabla base, en México la
app puede correr una hora sin una sola detección y ese es el resultado correcto,
no un fallo.

Incluir cómo subir el archivo por WiFi con el `web_file_browser` que Minino ya
tiene, sin sacar la tarjeta.

- [ ] **Step 3: Validación de campo con dos placas**

Placa A (emisor): firmware con un modo de prueba que transmite con
`esp_wifi_80211_tx` una probe request con SSID IE de longitud 0, `addr2` con un
OUI de la tabla y los IE de `FLOCK_IES`. Reutilizar el patrón de
`main/apps/wifi/ssid_spam` y `components/drone_id/spoofer`.

Placa B (detector): la app de vigilancia en perfil `Flock/ALPR`.

No se depende de que una sola placa se reciba a sí misma: la radio es
half-duplex y no está previsto que funcione.

- [ ] **Step 4: Verificar los criterios de aceptación del spec**

| Criterio | Cómo se mide | Umbral |
|---|---|---|
| Falsos positivos, tabla base, 30 min en zona urbana de México | App en perfil `Vigilancia`, contar filas de clase FLOCK/ALPR en el CSV | 0 |
| Detección de trama tier-4 sintética con hop activo | Placa A emitiendo cada segundo; cronómetro desde el arranque de B | < 2 s |
| Overflow del ring buffer en 1 h | `surv_queue_overflows()` en la pantalla de debug | 0 |
| Bloqueo del drenaje por escritura a microSD | Log de `esp_timer` alrededor de `sd_card_append_to_file` | < 50 ms |
| App de trackers tras el refactor | Verificación manual de la Task 5 | Sin regresión |

Cualquier criterio que falle vuelve a la tarea que lo causó. El de falsos
positivos, si falla, casi siempre apunta a una keyword de SSID demasiado
genérica en la Task 3.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance docs/surveillance-signatures.md
git commit -m "feat(surveillance): load signature overlay from microSD and document format"
```
