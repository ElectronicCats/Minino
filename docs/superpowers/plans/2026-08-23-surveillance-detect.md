# Surveillance Detector for Minino - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a passive surveillance infrastructure detector to Minino (Flock/ALPR cameras, body cams, trackers, IP cameras, drones, skimmers) that alerts locally and logs every detection with GPS coordinates to microSD.

**Architecture:** A pure C `surveillance_detect` component with matchers and the scoring engine isolated from all hardware (so they run as host tests on Linux), plus a menu app that handles only UI and persistence. The WiFi promiscuous callback and BLE GAP callback only compare and enqueue into a ring buffer; a separate task drains, scores, and notifies.

**Tech Stack:** ESP-IDF v5.5.1, target `esp32c6`, C99. Host tests with `gcc` and the Unity framework vendored in ESP-IDF, bypassing the IDF build system. Existing Minino components: `sd_card`, `nmea_parser`, `buzzer`, `leds`, `preferences`, `oled_screen`, `menus_module`, `espressif__pcap`.

**Spec:** `docs/superpowers/specs/2026-08-23-surveillance-detect-design.md`

**Branch:** `feat/surveillance-detect`

## Global Constraints

All values come directly from the spec and apply across all tasks.

- **ESP-IDF v5.5.1** at `/home/sabas/esp/esp-idf-v5.5.1`, target `esp32c6`.
  Activate with `. /home/sabas/esp/esp-idf-v5.5.1/export.sh` before running any
  `idf.py` command. Verified baseline: the firmware compiles cleanly with this version.
- **Host tests: direct `gcc` against the vendored Unity** at
  `$IDF_PATH/components/unity/unity/src`. Do **not** use the ESP-IDF `linux`
  target: it requires `libbsd-dev`, which is not installed and requires `sudo`
  to install. Verified with an end-to-end probe.
- **`surv_signatures.c`, `surv_match.c`, `surv_ie.c`, `surv_overlay.c`, and
  `surv_engine.c` must not include ESP-IDF headers.** The `gcc` build fails if
  they do, which guarantees that tests run without hardware.
- **GPL-3.0 license.** Every file derived from flock-you or eye-spy carries a header with `SPDX-License-Identifier: GPL-3.0-or-later` and a provenance notice citing the original project and its license (MIT / Apache-2.0).
- **`surv_` prefix** on all component symbols. Never `sd_` (collides with `sd_card`).
- **No `printf`, `malloc`, or file I/O inside radio callbacks.** Comparison and enqueuing only.
- **Ring buffer:** 32 entries.
- **Device table:** 200 MACs maximum.
- **Score:** decay -1 every **60 s**; re-score cooldown **120 s** per class; CLEAR 0-2, CAUTION 3-5, ALERT 6+.
- **Emission dedupe:** 5 s per MAC; a higher tier preempts it.
- **Channels:** 11/6/1 descending, dwell **250 ms**; in the `Surveillance` profile it extends to 13/8/3 every 4 passes. Always clamped to the region's `nchan`.
- **RSSI threshold:** frames and advertisements below **-95 dBm** are discarded. (`RSSI_MIN` value from flock-you. The spec left it unset; it is fixed here.)
- **CSV:** up to **200 bytes** per line, **200-line** buffer, `surveil/` directory.
- **pcap:** **4 MB** cap per session.
- **Overlay:** caps of **256 OUIs, 64 keywords, 8 IE signatures**, plus **16 UUIDs**
  (the spec did not set a UUID cap; it is fixed here).
- **Base table:** 75 OUIs (35 Flock, 6 contract manufacturer, 1 SoundThinking, 1 ALPR, 31 cameras, 1 Axon).
  The spec says 74 when counting only eye-spy's five WiFi tables; Axon is detected there
  by BLE MAC OUI outside those tables. Here it lives in the same unified table, queried
  by both the WiFi and BLE paths.
- **Do not filter locally administered MACs.** `82:6b:f2` has that bit set and is a confirmed Flock camera.

---

## File Structure

### New Component: `firmware/components/surveillance_detect/`

| File | Responsibility |
|---|---|
| `include/surv_types.h` | Enums (`surv_class_t`, `surv_proto_t`, `surv_tier_t`) and `surv_event_t`. No dependencies |
| `include/surv_signatures.h`, `surv_signatures.c` | Base table of OUIs, keywords, BLE names, UUIDs, IE signature. Accessors |
| `include/surv_match.h`, `surv_match.c` | Pure matchers: OUI with bitmap prefilter, SSID keyword, BLE name, advertising dissector |
| `include/surv_ie.h`, `surv_ie.c` | Information Element fingerprint. Isolated because it is the most delicate piece |
| `include/surv_overlay.h`, `surv_overlay.c` | microSD overlay parser. Accepts text in memory, does not touch the filesystem |
| `include/surv_engine.h`, `surv_engine.c` | Score, decay, cooldown, per-MAC table, dedupe |
| `surv_radio.c` | Phase scheduler and channel hopping. Depends on ESP-IDF |
| `include/surveillance_detect.h`, `surveillance_detect.c` | Public API, radio callbacks, ring buffer. Depends on ESP-IDF |
| `test/` | Host test harness: `Makefile`, `surv_test.h` shim, runner, and test files |

The first six do not include any ESP-IDF headers beyond `stdint.h`/`stdbool.h`/`string.h`. That is the condition for running tests on the host, and the `gcc` build in `test/Makefile` enforces it: if anyone includes `esp_wifi.h` there, the tests fail to compile.

### New App: `firmware/main/apps/surveillance/`

| File | Responsibility |
|---|---|
| `surveillance_module.c/.h` | Lifecycle, button callbacks, orchestration |
| `surveillance_screens.c/.h` | OLED screens |
| `surveillance_log.c/.h` | CSV, GPX, and pcap writing to microSD |

`firmware/main/CMakeLists.txt` uses `file(GLOB_RECURSE ...)` across `main/`, so **new files do not need manual registration**.

### Modified Existing Files

| File | Change |
|---|---|
| `firmware/components/trackers_scanner/trackers_scanner.c` | Delegate the dissector to `surv_match`; switch to passive scan |
| `firmware/components/bt_gattc/bt_gattc.c`, `bt_gattc.h` | Allow passive scan without changing the default for other consumers |
| `firmware/components/radio_selector/include/radio_selector.h`, `radio_selector.c` | `RADIO_SELECT_SURVEILLANCE` and its setter |
| `firmware/main/modules/menus_module/menus_include/menus.h` | Menu entry |
| `firmware/main/modules/settings/` | Radio profile, active scan toggle, buzzer mask |
| `.github/workflows/` | New workflow for host tests |

---

## Task 1: Component Skeleton, Types, and Host Test App

**Files:**
- Create: `firmware/components/surveillance_detect/CMakeLists.txt`
- Create: `firmware/components/surveillance_detect/include/surv_types.h`
- Create: `firmware/components/surveillance_detect/surv_signatures.c`
- Create: `firmware/components/surveillance_detect/include/surv_signatures.h`
- Create: `firmware/components/surveillance_detect/test/Makefile`
- Create: `firmware/components/surveillance_detect/test/surv_test.h`
- Create: `firmware/components/surveillance_detect/test/surv_test_main.c`
- Create: `firmware/components/surveillance_detect/test/test_surv_signatures.c`
- Create: `.github/workflows/host-tests.yml`

**Interfaces:**
- Consumes: nothing.
- Produces: `surv_class_t`, `surv_proto_t`, `surv_event_t`, `SURV_TIER_*`; `surv_signatures_oui_count()`.

- [ ] **Step 1: Write the failing test**

`test/test_surv_signatures.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_test.h"
#include "surv_signatures.h"
#include "surv_types.h"

TEST_CASE("base table has 75 OUIs", "[surv][signatures]") {
  TEST_ASSERT_EQUAL_UINT16(75, surv_signatures_oui_count());
}
```

- [ ] **Step 2: Create the host test harness**

Three files. The `surv_test.h` shim reproduces ESP-IDF's `TEST_CASE` macro on top of vanilla Unity using constructor auto-registration, so test files are written just like in `components/console/test_apps` but run with `gcc`, without ESP-IDF or `libbsd-dev`.

`test/surv_test.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "unity.h"

typedef void (*surv_test_fn)(void);
void surv_test_register(const char* name, const char* tags, surv_test_fn fn);

#define SURV_CAT_(a, b) a##b
#define SURV_CAT(a, b) SURV_CAT_(a, b)

// Declares the test and a constructor that registers it before main().
#define TEST_CASE(desc, tags)                                        \
  static void SURV_CAT(surv_test_, __LINE__)(void);                  \
  __attribute__((constructor)) static void SURV_CAT(surv_reg_,       \
                                                    __LINE__)(void) {\
    surv_test_register(desc, tags, SURV_CAT(surv_test_, __LINE__));  \
  }                                                                  \
  static void SURV_CAT(surv_test_, __LINE__)(void)
```

`test/surv_test_main.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdio.h>
#include "surv_test.h"

#define SURV_TEST_MAX 256

static struct {
  const char*  name;
  const char*  tags;
  surv_test_fn fn;
} s_t[SURV_TEST_MAX];
static int s_n;

void surv_test_register(const char* name, const char* tags, surv_test_fn fn) {
  if (s_n < SURV_TEST_MAX) {
    s_t[s_n].name = name;
    s_t[s_n].tags = tags;
    s_t[s_n].fn = fn;
    s_n++;
  }
}

void setUp(void) {}
void tearDown(void) {}

int main(void) {
  UNITY_BEGIN();
  for (int i = 0; i < s_n; i++) {
    UnityDefaultTestRun(s_t[i].fn, s_t[i].name, 0);
  }
  return UNITY_END();
}
```

`test/Makefile` - `TESTS` is the list to which each subsequent task appends its test file:

```make
# SPDX-License-Identifier: GPL-3.0-or-later
# Host tests: gcc against the Unity vendored in ESP-IDF.
# Requires IDF_PATH (. $IDF_PATH/export.sh) but does NOT use the IDF build system.
UNITY  := $(IDF_PATH)/components/unity/unity/src
SRCDIR := ..

SRCS := $(SRCDIR)/surv_signatures.c \
        $(SRCDIR)/surv_match.c \
        $(SRCDIR)/surv_ie.c \
        $(SRCDIR)/surv_overlay.c \
        $(SRCDIR)/surv_engine.c

TESTS := test_surv_signatures.c

CFLAGS := -std=gnu99 -Wall -Wextra -Werror -g -I. -I$(SRCDIR)/include -I$(UNITY)

run-tests: build/run-tests
	./build/run-tests

build/run-tests: $(SRCS) $(TESTS) surv_test_main.c surv_test.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(TESTS) surv_test_main.c $(UNITY)/unity.c -o $@

clean:
	rm -rf build

.PHONY: run-tests clean
```

Tabs in Makefile recipes are mandatory: if the editor converts them to spaces, `make` fails with `missing separator`.

- [ ] **Step 3: Write the types**

`include/surv_types.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Shared types for the surveillance detector.
//
// Provenance: the surv_class_t taxonomy derives from the detection engines in
// eye-spy (simeononsecurity, Apache-2.0), specifically the DECL_DETECTOR macros
// in src/es_confidence.h. The SURV_TIER_* tier levels derive from flock-you
// (colonelpanichacks, MIT).
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  SURV_CLASS_NONE = 0,
  SURV_CLASS_FLOCK,       // Flock Safety camera
  SURV_CLASS_FLOCK_MFR,   // contract manufacturer OUI, low confidence
  SURV_CLASS_ALPR,        // generic license plate reader
  SURV_CLASS_SOUNDTHINKING,
  SURV_CLASS_AXON,        // body cam
  SURV_CLASS_GLASSES,     // Ray-Ban Meta
  SURV_CLASS_CAM,         // consumer IP camera
  SURV_CLASS_AIRTAG,
  SURV_CLASS_SMARTTAG,
  SURV_CLASS_TILE,
  SURV_CLASS_APPLE_NEARBY,
  SURV_CLASS_IBEACON,
  SURV_CLASS_ODID,        // drone Remote ID
  SURV_CLASS_SKIMMER,
  SURV_CLASS_MESHCORE,
  SURV_CLASS_RAVEN,
  SURV_CLASS_PERSIST,     // unknown MAC seen repeatedly
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
  uint8_t      channel;  // 0 if BLE
  surv_proto_t proto;
} surv_event_t;
```

- [ ] **Step 4: Write the base table and its accessor**

`include/surv_signatures.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Signature tables. Provenance:
//   - Flock OUIs: @NitekryDPaul (nite-oui-collection) via flock-you (MIT)
//     and DeFlockJoplin (82:6b:f2).
//   - Camera, ALPR, SoundThinking OUIs and BLE signatures: eye-spy (Apache-2.0).
#pragma once
#include <stdint.h>
#include "surv_types.h"

typedef struct {
  uint8_t      oui[3];
  surv_class_t klass;
  uint8_t      points;  // weight in global score
  uint8_t      tier;    // confidence CEILING for this signature, not the event tier
} surv_oui_entry_t;

typedef struct {
  const char*  kw;
  surv_class_t klass;
  uint8_t      points;
} surv_kw_entry_t;

typedef struct {
  uint16_t     uuid;    // 16-bit service UUID
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

// Rebuilds the effective table = base + added - removed. Called by the
// overlay (Task 8). With extra_count and removed_count at 0, returns the base table.
void surv_signatures_build_effective(const surv_oui_entry_t* extra,
                                     uint16_t extra_count,
                                     const uint8_t (*removed)[3],
                                     uint16_t removed_count);
void surv_signatures_build_effective_kws(const surv_kw_entry_t* extra,
                                         uint16_t extra_count);
void surv_signatures_build_effective_uuids(const surv_uuid_entry_t* extra,
                                           uint16_t extra_count);

// Overlay caps. Single source of truth: here they size the effective arrays
// and in surv_overlay.c they limit how many entries are accepted.
#define SURV_OVERLAY_MAX_OUIS  256
#define SURV_OVERLAY_MAX_KWS   64
#define SURV_OVERLAY_MAX_UUIDS 16
```

`surv_signatures.c` - the 75 entries. Excerpt showing the exact structure; the remaining entries are transcribed from `eye-spy/src/es_detect.h` preserving the class and weight of each source table:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_signatures.h"

static const surv_oui_entry_t OUIS[] = {
    // --- Flock Safety (@NitekryDPaul, 25 entries) --- class FLOCK, +5, tier ceiling 4
    {{0x70, 0xc9, 0x4e}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x91, 0x80}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // ... 23 more
    // --- DeFlockJoplin: LAA bit set, DO NOT filter ---
    {{0x82, 0x6b, 0xf2}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Direct IEEE assignment to Flock Safety ---
    {{0xb4, 0x1e, 0x52}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- FS Ext Battery (6 entries) ---
    {{0x04, 0x0d, 0x84}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // ... 5 more
    // --- Legacy eye-spy entries ---
    {{0xd4, 0xbb, 0xe6}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    {{0x3c, 0x61, 0x05}, SURV_CLASS_FLOCK, 5, SURV_TIER_IE_SIG},
    // --- Contract manufacturer (Liteon/USI, 6) --- tier ceiling 1: shared hardware
    {{0xf4, 0x6a, 0xdd}, SURV_CLASS_FLOCK_MFR, 2, SURV_TIER_ADDR13},
    // ... 5 more
    // --- SoundThinking / ShotSpotter (1) ---
    {{0xd4, 0x11, 0xd6}, SURV_CLASS_SOUNDTHINKING, 4, SURV_TIER_ADDR2},
    // --- ALPR: Motorola Solutions / Vigilant (1) ---
    {{0x00, 0x0e, 0x58}, SURV_CLASS_ALPR, 5, SURV_TIER_ADDR2},
    // --- IP Cameras (31) ---
    {{0x00, 0x40, 0x8c}, SURV_CLASS_CAM, 3, SURV_TIER_ADDR2},  // Axis
    // ... 30 more
    // --- Axon: BLE MAC OUI (body cams, tasers, police gear) ---
    // In eye-spy this lives outside the OUI tables and is matched against the
    // advertisement BDA. Here it goes into the same table, queried by both paths.
    {{0x00, 0x25, 0xdf}, SURV_CLASS_AXON, 5, SURV_TIER_ADDR2},
};

uint16_t surv_signatures_oui_count(void) {
  return (uint16_t) (sizeof(OUIS) / sizeof(OUIS[0]));
}

const surv_oui_entry_t* surv_signatures_ouis(void) {
  return OUIS;
}
```

Component `CMakeLists.txt`:

```cmake
idf_component_register(SRCS "surv_signatures.c"
                            "surv_match.c"
                            "surv_ie.c"
                            "surv_overlay.c"
                            "surv_engine.c"
                            "surv_radio.c"
                            "surveillance_detect.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi bt esp_timer radio_selector)
```

The component only compiles for `esp32c6`; host tests do not go through CMake. The "no ESP-IDF headers" discipline across the five pure files is enforced by the `gcc` build in `test/Makefile`, which fails if anyone violates it.

**All** listed `.c` files are created in this task. The four remaining pure files (`surv_match.c`, `surv_ie.c`, `surv_overlay.c`, `surv_engine.c`) start empty except for `#include` of their header; their implementation arrives in Tasks 2, 6, 8, and 7.

`surv_radio.c` and `surveillance_detect.c` start with **no-op stubs** returning `ESP_OK` or zero so `idf.py build` passes in Step 6 of this task; Tasks 9 and 10 replace them:

```c
// surv_radio.c - stub; real implementation arrives in Task 10.
#include "surv_radio.h"
esp_err_t surv_radio_start(surv_profile_t p, bool active_scan) {
  (void) p; (void) active_scan; return ESP_OK;
}
void    surv_radio_stop(void) {}
uint8_t surv_radio_current_channel(void) { return 0; }
```

`surveillance_detect.c` gets the same treatment for `surv_begin`, `surv_stop`, `surv_register_cb`, and `surv_queue_overflows`.

Headers `include/surv_radio.h` and `include/surveillance_detect.h` are created here **with these exact signatures**, matching what Tasks 9 and 10 declare. Do not invent others: if they diverge, Task 9 will rewrite headers instead of filling in implementations.

```c
// include/surveillance_detect.h
typedef enum {
  SURV_PROFILE_FLOCK = 0,   // 100% promiscuous WiFi
  SURV_PROFILE_SURVEIL,     // BLE + WiFi cycle
  SURV_PROFILE_TRACKERS,    // 100% passive BLE
} surv_profile_t;

typedef void (*surv_detect_cb_t)(const surv_event_t* ev, uint8_t score);

esp_err_t surv_begin(surv_profile_t profile, bool active_scan);
void      surv_stop(void);
void      surv_register_cb(surv_detect_cb_t cb);
uint32_t  surv_queue_overflows(void);

// include/surv_radio.h
esp_err_t surv_radio_start(surv_profile_t p, bool active_scan);
void      surv_radio_stop(void);
uint8_t   surv_radio_current_channel(void);
```

The profile describes **what is being hunted**, not which radio is turned on: a `SURV_PROFILE_BLE`/`SURV_PROFILE_WIFI` enum would be a different and mistaken concept.

- [ ] **Step 5: Run the host test and verify it passes**

```bash
. /home/sabas/esp/esp-idf-v5.5.1/export.sh
cd firmware/components/surveillance_detect/test
make run-tests
```

Expected output: `1 Tests 0 Failures 0 Ignored` and `OK`. `make` returns a non-zero exit code if any test fails, making it suitable for CI.

Expected: test `base table has 75 OUIs` passes. If it fails with a different count, entries are missing from the `es_detect.h` transcription.

- [ ] **Step 6: Verify the firmware still compiles for the real target**

```bash
. /home/sabas/esp/esp-idf-v5.5.1/export.sh
cd firmware
idf.py build
```

Expected: build OK. Nothing uses the new component yet, but it must compile. The baseline was verified green before starting the plan, so any failure here was introduced by this task.

- [ ] **Step 7: Create the host test workflow**

`.github/workflows/host-tests.yml`. The existing `builds.yml` workflow only triggers on release tags and does not run tests, so this workflow is new:

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
      - name: Fetch Unity from ESP-IDF
        run: |
          git clone --depth 1 -b v5.5.1 --filter=blob:none --sparse \
              https://github.com/espressif/esp-idf.git "$RUNNER_TEMP/esp-idf"
          git -C "$RUNNER_TEMP/esp-idf" sparse-checkout set components/unity
      - name: Run surveillance_detect host tests
        run: make -C firmware/components/surveillance_detect/test run-tests
        env:
          IDF_PATH: ${{ runner.temp }}/esp-idf
```

CI does not install the ESP-IDF toolchain: it only needs `gcc` and the two Unity files, so it fetches the IDF repository with sparse-checkout and runs in seconds.

- [ ] **Step 8: Commit**

```bash
git add firmware/components/surveillance_detect .github/workflows/host-tests.yml
git commit -m "feat(surveillance): add component skeleton, signature tables and host tests"
```

---

## Task 2: OUI Matcher with Bitmap Prefilter

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`
- Create: `firmware/components/surveillance_detect/include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/test/CMakeLists.txt`
- Test: `firmware/components/surveillance_detect/test/test_surv_match.c`

**Interfaces:**
- Consumes: `surv_signatures_ouis()`, `surv_signatures_oui_count()`, `surv_oui_entry_t`.
- Produces: `void surv_match_init(void)`, `const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6])`.

- [ ] **Step 1: Write the failing tests**

`test/test_surv_match.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_match.h"
#include "surv_signatures.h"
#include "surv_test.h"

TEST_CASE("Flock OUI matches with class FLOCK", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("unknown MAC does not match", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("locally administered MAC 82:6b:f2 DOES match", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0x82, 0x6b, 0xf2, 0x14, 0x07, 0x3a};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("contract manufacturer OUI has tier ceiling 1", "[surv][match]") {
  surv_match_init();
  const uint8_t mac[6] = {0xf4, 0x6a, 0xdd, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK_MFR, e->klass);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_ADDR13, e->tier);
}

TEST_CASE("no OUI appears in two distinct classes", "[surv][match]") {
  const surv_oui_entry_t* t = surv_signatures_ouis();
  uint16_t n = surv_signatures_oui_count();
  for (uint16_t i = 0; i < n; i++) {
    for (uint16_t j = i + 1; j < n; j++) {
      TEST_ASSERT_FALSE_MESSAGE(memcmp(t[i].oui, t[j].oui, 3) == 0,
                                "duplicate OUI in base table");
    }
  }
}
```

Add `test_surv_match.c` to the `TESTS` variable in `firmware/components/surveillance_detect/test/Makefile`.

- [ ] **Step 2: Run and verify it fails**

```bash
make -C firmware/components/surveillance_detect/test run-tests
```

Expected: compilation error, `surv_match.h` does not exist.

- [ ] **Step 3: Write the minimal implementation**

`include/surv_match.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "surv_signatures.h"
#include "surv_types.h"

// Builds the prefilter. Call once before any match.
void surv_match_init(void);

// Returns the table entry or NULL. Safe to call from the promiscuous
// callback: no memory allocation, no I/O.
const surv_oui_entry_t* surv_match_oui(const uint8_t mac[6]);
```

`surv_match.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include <string.h>

// Prefilter: one bit per possible first octet. Discards 99% of frames
// in a single operation, allowing this to run inside the WiFi callback.
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

- [ ] **Step 4: Run and verify they pass**

```bash
make -C firmware/components/surveillance_detect/test run-tests
```

Expected: all five tests pass. If the duplicate OUI test fails, an entry was transcribed into two different tables in `es_detect.h`: fix the table, not the test.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add OUI matcher with first-octet bitmap prefilter"
```

---

## Task 3: SSID Keyword and BLE Name Matchers

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`, `include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/surv_signatures.c`, `include/surv_signatures.h`
- Test: `firmware/components/surveillance_detect/test/test_surv_match.c`

**Interfaces:**
- Consumes: `surv_signatures_kws()`, `surv_signatures_kw_count()`.
- Produces: `const surv_kw_entry_t* surv_match_ssid(const char* ssid)`, `bool surv_match_contains_ci(const char* hay, const char* needle)`.

- [ ] **Step 1: Write the failing tests**

Add to `test_surv_match.c`:

```c
TEST_CASE("substring search is case-insensitive", "[surv][match]") {
  TEST_ASSERT_TRUE(surv_match_contains_ci("FLOCK_CAM_0032", "flock"));
  TEST_ASSERT_TRUE(surv_match_contains_ci("my-FlOcK-net", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("floc", "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci(NULL, "flock"));
  TEST_ASSERT_FALSE(surv_match_contains_ci("flock", NULL));
}

TEST_CASE("Flock SSID matches with 5 points and tier 0", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("Flock_CAM_0032");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}

TEST_CASE("camera SSID matches with 2 points", "[surv][match]") {
  const surv_kw_entry_t* e = surv_match_ssid("home-ipcam-01");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
  TEST_ASSERT_EQUAL_UINT8(2, e->points);
}

TEST_CASE("ordinary SSID does not match", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("INFINITUM1234"));
}

// Regression: these two violated the zero-false-positive ALPR criterion.
TEST_CASE("consumer Motorola modem is NOT ALPR", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("MOTOROLA-A1B2"));
}

TEST_CASE("LPR printer is NOT ALPR", "[surv][match]") {
  TEST_ASSERT_NULL(surv_match_ssid("HP_LPR_Printer"));
}
```

- [ ] **Step 2: Run and verify it fails**

Run: `make -C firmware/components/surveillance_detect/test run-tests`. Expected: `surv_match_ssid` undeclared.

- [ ] **Step 3: Add the keyword table**

In `surv_signatures.c`, transcribed from `FLOCK_SSID_KW`, `ALPR_SSID_KW`, and `CAM_SSID_KW` in eye-spy. **Order matters: most specific to least specific**, so `"flocksafety"` wins before `"cam"` catches a generic match:

```c
static const surv_kw_entry_t KWS[] = {
    {"flocksafety", SURV_CLASS_FLOCK, 5}, {"flock", SURV_CLASS_FLOCK, 5},
    {"pigvision", SURV_CLASS_FLOCK, 5},   {"penguin", SURV_CLASS_FLOCK, 5},
    {"fs ext", SURV_CLASS_FLOCK, 5},      {"raven", SURV_CLASS_RAVEN, 5},
    // "motorola" and "lpr" are DELIBERATELY omitted despite appearing in
    // eye-spy's ALPR_SSID_KW: they break the zero-false-positive acceptance
    // criterion for class ALPR. "motorola" matches the default SSID of any
    // consumer Motorola/Arris modem, deployed by the millions in Mexico by
    // Izzi and Totalplay; "lpr" matches inside "HelpRouter" and "HP_LPR_*"
    // printers. Anyone who wants them can re-add them via microSD overlay.
    {"licenseplat", SURV_CLASS_ALPR, 4},  {"plateread", SURV_CLASS_ALPR, 4},
    {"vigilant", SURV_CLASS_ALPR, 4},     {"automate", SURV_CLASS_ALPR, 4},
    {"alpr", SURV_CLASS_ALPR, 4},
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

- [ ] **Step 4: Implement the matchers**

In `surv_match.c`:

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

Declare both in `surv_match.h`.

- [ ] **Step 5: Run and verify they pass**

Run: `make -C firmware/components/surveillance_detect/test run-tests`.
Expected: all nine tests pass.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add case-insensitive SSID keyword matcher"
```

---

## Task 4: Shared BLE Advertising Dissector

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_match.c`, `include/surv_match.h`
- Modify: `firmware/components/surveillance_detect/surv_signatures.c`, `include/surv_signatures.h`
- Test: `firmware/components/surveillance_detect/test/test_surv_ble.c`

**Interfaces:**
- Consumes: `surv_match_contains_ci()`, `surv_signatures_uuids()`, `surv_signatures_uuid_count()`, `surv_signatures_ble_names()`, `surv_signatures_ble_name_count()`, `surv_signatures_skimmers()`, `surv_signatures_skimmer_count()` (all declared in Task 1).
- Produces:
  ```c
  #define SURV_BLE_MAX_HITS 4
  typedef struct {
    surv_class_t klass;
    uint8_t      points;
    const char*  label;   // "AirTag", "Axon", ... static string
  } surv_ble_hit_t;
  uint8_t surv_match_ble_adv(const uint8_t* adv, uint8_t adv_len,
                             surv_ble_hit_t out[SURV_BLE_MAX_HITS]);
  ```

Key difference from `trackers_scanner`, which breaks on the first match: this function returns **up to 4 classes** from the same advertisement, because a device can simultaneously be an iBeacon and something else.

- [ ] **Step 1: Write the failing tests**

`test/test_surv_ble.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_match.h"
#include "surv_test.h"

TEST_CASE("detects an AirTag by mfr data 004C subtype 12", "[surv][ble]") {
  // len=0x06 (AD type + 5 payload bytes), AD type=0xFF,
  // company=0x004C (LE), subtype=0x12.
  // NOTE: the length byte must match the actual buffer, not the full 31-byte
  // advertisement emitted by a real AirTag; otherwise the bounds guard
  // (correctly) rejects it and the test fails.
  const uint8_t adv[] = {0x06, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AIRTAG, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(4, hits[0].points);
}

TEST_CASE("detects an iBeacon by 4C 00 02 15", "[surv][ble]") {
  // len=0x09 (AD type + 8 payload bytes)
  const uint8_t adv[] = {0x09, 0xff, 0x4c, 0x00, 0x02, 0x15,
                         0x00, 0x00, 0x00, 0x00};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_IBEACON, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(2, hits[0].points);
}

TEST_CASE("detects Tile by service UUID FEED", "[surv][ble]") {
  // len=0x03, AD type=0x16 (service data), UUID 0xFEED little-endian
  const uint8_t adv[] = {0x03, 0x16, 0xed, 0xfe};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_TILE, hits[0].klass);
}

TEST_CASE("detects Ray-Ban Meta by service UUID FD5F", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0x5f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_GLASSES, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("detects BLE ODID by service UUID FFFA", "[surv][ble]") {
  const uint8_t adv[] = {0x03, 0x03, 0xfa, 0xff};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_ODID, hits[0].klass);
}

TEST_CASE("detects an HC-05 skimmer by exact name", "[surv][ble]") {
  // len=0x06, AD type=0x09 (complete name), "HC-05"
  const uint8_t adv[] = {0x06, 0x09, 'H', 'C', '-', '0', '5'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_SKIMMER, hits[0].klass);
}

TEST_CASE("detects Flock BLE by name substring", "[surv][ble]") {
  const uint8_t adv[] = {0x0f, 0x09, 'F', 'S', ' ', 'E', 'x', 't',
                         ' ',  'B',  'a', 't', 't', 'e', 'r', 'y'};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, sizeof(adv), hits);
  TEST_ASSERT_EQUAL_UINT8(1, n);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, hits[0].klass);
  TEST_ASSERT_EQUAL_UINT8(5, hits[0].points);
}

TEST_CASE("truncated advertisement does not overflow", "[surv][ble]") {
  // declared length larger than actual buffer
  const uint8_t adv[] = {0x1f, 0xff, 0x4c};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("advertisement with no known signature returns 0", "[surv][ble]") {
  const uint8_t adv[] = {0x02, 0x01, 0x06};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(0, surv_match_ble_adv(adv, sizeof(adv), hits));
}

TEST_CASE("detects Axon by BLE MAC OUI", "[surv][ble]") {
  surv_match_init();
  const uint8_t bda[6] = {0x00, 0x25, 0xdf, 0x01, 0x02, 0x03};
  const surv_oui_entry_t* e = surv_match_oui(bda);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_AXON, e->klass);
  TEST_ASSERT_EQUAL_UINT8(5, e->points);
}
```

Add `test_surv_ble.c` to the `TESTS` variable in `test/Makefile`.

- [ ] **Step 2: Run and verify it fails**

Run: `make -C firmware/components/surveillance_detect/test run-tests`. Expected: `surv_match_ble_adv` undeclared.

- [ ] **Step 3: Add BLE signatures to `surv_signatures.c`**

```c
// 16-bit UUIDs, from eye-spy (Apache-2.0)
static const surv_uuid_entry_t UUIDS[] = {
    {0xFD5F, SURV_CLASS_GLASSES,  5, "RayBan Meta"},
    {0xFFFA, SURV_CLASS_ODID,     4, "Drone ODID"},
    {0xFD5A, SURV_CLASS_SMARTTAG, 3, "SmartTag"},
    {0xFEED, SURV_CLASS_TILE,     3, "Tile"},
    {0xFEEC, SURV_CLASS_TILE,     3, "Tile"},
};

// BLE name substrings, case-insensitive
static const surv_name_entry_t BLE_NAMES[] = {
    {"fs ext battery", SURV_CLASS_FLOCK,    5, "Flock Battery"},
    {"pigvision",      SURV_CLASS_FLOCK,    5, "Flock"},
    {"penguin",        SURV_CLASS_FLOCK,    5, "Flock"},
    {"flock",          SURV_CLASS_FLOCK,    5, "Flock"},
    {"raven",          SURV_CLASS_RAVEN,    5, "Raven"},
    {"meshcore-",      SURV_CLASS_MESHCORE, 2, "MeshCore"},
};

// Exact names of skimmer modules
static const char* const SKIMMER_NAMES[] = {"HC-03", "HC-05", "HC-06"};
```

Write the six accessors declared in Task 1 (`surv_signatures_uuids`, `surv_signatures_uuid_count`, `surv_signatures_ble_names`, `surv_signatures_ble_name_count`, `surv_signatures_skimmers`, `surv_signatures_skimmer_count`) with the same trivial body as `surv_signatures_ouis()`.

- [ ] **Step 4: Implement the dissector**

In `surv_match.c`. Walks standard BLE TLVs in a single pass and accumulates hits:

```c
static void push_hit(surv_ble_hit_t* out, uint8_t* n, surv_class_t k,
                     uint8_t pts, const char* label) {
  if (*n >= SURV_BLE_MAX_HITS) {
    return;
  }
  for (uint8_t i = 0; i < *n; i++) {
    if (out[i].klass == k) {
      return;  // already recorded
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
      break;  // impossible length: truncated advertisement
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
      } else if (company == 0x09C8) {  // XUNTONG, confirmed Flock (eye-spy)
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

    if (ad_type == 0x08 || ad_type == 0x09) {  // shortened / complete local name
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

- [ ] **Step 5: Run and verify they pass**

Run: `make -C firmware/components/surveillance_detect/test run-tests`.
Expected: all tests in `[surv][ble]` pass.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add shared BLE advertising dissector"
```

---

## Task 5: Refactor `trackers_scanner` onto the Shared Dissector

This is the only task that touches code currently working in users' hands. It goes early and isolated, and ends with mandatory manual verification on hardware.

**Files:**
- Modify: `firmware/components/trackers_scanner/trackers_scanner.c:115-185`
- Modify: `firmware/components/trackers_scanner/CMakeLists.txt`
- Modify: `firmware/components/bt_gattc/bt_gattc.c:55-70`, `firmware/components/bt_gattc/include/bt_gattc.h`

**Interfaces:**
- Consumes: `surv_match_ble_adv()`, `surv_ble_hit_t`, `SURV_BLE_MAX_HITS`.
- Produces: `void bt_gattc_set_passive_scan(bool passive)` - affects only subsequent calls to `bt_gattc_set_ble_scan_params()`, leaving the default unchanged for other consumers.

- [ ] **Step 1: Add passive scan mode to `bt_gattc`**

In `bt_gattc.c`, next to the current scan params definition (`bt_gattc.c:61` has `.scan_type = BLE_SCAN_TYPE_ACTIVE`):

```c
static bool s_passive_scan = false;

void bt_gattc_set_passive_scan(bool passive) {
  s_passive_scan = passive;
}
```

and in `bt_gattc_set_default_ble_scan_params()` replace the literal with:

```c
.scan_type = s_passive_scan ? BLE_SCAN_TYPE_PASSIVE : BLE_SCAN_TYPE_ACTIVE,
```

Declare `bt_gattc_set_passive_scan` in `bt_gattc.h`.

- [ ] **Step 2: Replace the body of `tracker_dissector`**

In `trackers_scanner.c`, replace the full TLV loop (the `while` in `tracker_dissector`) with a call to the shared dissector, preserving the function signature and `tracker_record` population:

```c
static void tracker_dissector(esp_ble_gap_cb_param_t* scan_rst,
                              tracker_profile_t* tracker_record) {
  uint8_t* adv = scan_rst->scan_rst.ble_adv;
  uint8_t  adv_len = scan_rst->scan_rst.adv_data_len;

  tracker_record->is_tracker = false;

  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  uint8_t n = surv_match_ble_adv(adv, adv_len, hits);

  // This app only displays personal trackers. The remaining classes recognized
  // by the shared dissector (Flock, Axon, drones) are consumed by the
  // surveillance app, not this one.
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

Add `#include "surv_match.h"` and, in `trackers_scanner/CMakeLists.txt`, add `surveillance_detect` to `PRIV_REQUIRES`:

```cmake
idf_component_register(SRCS "trackers_scanner.c"
PRIV_REQUIRES bt bt_gattc surveillance_detect
INCLUDE_DIRS ".")
```

- [ ] **Step 3: Enable passive scan on scanner startup**

In `trackers_scanner_start()`, before `bt_gattc_set_ble_scan_params()`:

```c
bt_gattc_set_passive_scan(true);
```

- [ ] **Step 4: Compile for the real target**

```bash
cd firmware && idf.py set-target esp32c6 && idf.py build
```

Expected: build OK.

- [ ] **Step 5: Manual verification on hardware - mandatory**

```bash
cd firmware && idf.py -p /dev/ttyACM0 flash monitor
```

With an AirTag (or a Tile, or a Samsung phone with SmartTag) within 2 m:

1. Menu -> Bluetooth -> Trackers Scan.
2. Verify that the device appears in the list with its proper name.
3. Verify that the RSSI updates when moving it closer and further away.
4. Verify that exiting with BUTTON_LEFT returns to the menu without hanging.

If any of the four checks fail, **do not proceed**: the shared dissector has a regression relative to the original and must be fixed here.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/trackers_scanner firmware/components/bt_gattc
git commit -m "refactor(trackers): use shared BLE dissector and passive scanning"
```

---

## Task 6: Information Element Fingerprinting

The most delicate piece of the plan. Lives in its own file with its own test suite.

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_ie.h`
- Modify: `firmware/components/surveillance_detect/surv_ie.c`
- Test: `firmware/components/surveillance_detect/test/test_surv_ie.c`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```c
  typedef struct { uint8_t tag; uint8_t vlen; uint8_t vendor[7]; } surv_ie_tok_t;
  int  surv_ie_is_wildcard_probe(const uint8_t* ies, int len);
  bool surv_ie_matches_flock(const uint8_t* ies, int len);
  // Adds an alternative signature. Used by the overlay (Task 8) to track
  // cameras when Flock updates their firmware via OTA: a fleet may run mixed
  // versions, so up to SURV_IE_MAX_SIGS are supported simultaneously.
  // Returns false if the table is full.
  bool surv_ie_add_signature(const surv_ie_tok_t* toks, uint8_t count);
  // Discards overlay signatures and leaves only the compiled-in one.
  void surv_ie_reset_signatures(void);
  ```
  `surv_ie_is_wildcard_probe` returns 1 (length-0 SSID IE), 0 (non-empty SSID IE), or -1 (no SSID IE).

- [ ] **Step 1: Write the failing tests**

`test/test_surv_ie.c`. The vector is constructed to reproduce the exact drive-tested signature from flock-you `2,12,127,221:506f9a16030103,45,191,221:0050f208000000`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
// Frame vectors. The base vector is synthetic and reproduces the signature
// documented by DeFlockJoplin. It should be supplemented with real captures
// extracted from evidence pcaps (Task 15) once available.
#include <string.h>
#include "surv_ie.h"
#include "surv_test.h"

// Wildcard SSID + the seven IEs of the primary Flock signature
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

TEST_CASE("detects wildcard SSID IE", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(1, surv_ie_is_wildcard_probe(FLOCK_IES,
                                                     sizeof(FLOCK_IES)));
}

TEST_CASE("a directed probe is not a wildcard", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x04, 'h', 'o', 'm', 'e'};
  TEST_ASSERT_EQUAL_INT(0, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("without SSID IE returns -1", "[surv][ie]") {
  const uint8_t ies[] = {0x2d, 0x02, 0x00, 0x00};
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(ies, sizeof(ies)));
}

TEST_CASE("primary Flock signature matches", "[surv][ie]") {
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));
}

TEST_CASE("probe with different IEs does not match", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(ies, sizeof(ies)));
}

// Verifies that trailing bytes after the signature do not break it. Guaranteed
// by early exit once the signature completes, without needing any retry.
TEST_CASE("signature matches with 4 trailing FCS bytes", "[surv][ie]") {
  uint8_t ies[sizeof(FLOCK_IES) + 4];
  memcpy(ies, FLOCK_IES, sizeof(FLOCK_IES));
  memset(ies + sizeof(FLOCK_IES), 0xAB, 4);  // garbage FCS
  TEST_ASSERT_TRUE(surv_ie_matches_flock(ies, sizeof(ies)));
}

TEST_CASE("TLV with impossible length does not hang or overflow", "[surv][ie]") {
  const uint8_t ies[] = {0x00, 0x00, 0x2d, 0xff, 0x01, 0x02};
  (void) surv_ie_matches_flock(ies, sizeof(ies));  // must not crash
  TEST_PASS();
}

TEST_CASE("len 0 and null pointer are handled", "[surv][ie]") {
  TEST_ASSERT_EQUAL_INT(-1, surv_ie_is_wildcard_probe(NULL, 0));
  TEST_ASSERT_FALSE(surv_ie_matches_flock(NULL, 0));
}

TEST_CASE("added signature matches without losing the compiled-in one", "[surv][ie]") {
  surv_ie_reset_signatures();
  const uint8_t other_frame[] = {0x00, 0x00, 0x2d, 0x02, 0x00, 0x00,
                                 0xbf, 0x02, 0x00, 0x00};
  TEST_ASSERT_FALSE(surv_ie_matches_flock(other_frame, sizeof(other_frame)));

  const surv_ie_tok_t other[] = {{45, 0, {0}}, {191, 0, {0}}};
  TEST_ASSERT_TRUE(surv_ie_add_signature(other, 2));
  TEST_ASSERT_TRUE(surv_ie_matches_flock(other_frame, sizeof(other_frame)));
  // the compiled-in signature remains active
  TEST_ASSERT_TRUE(surv_ie_matches_flock(FLOCK_IES, sizeof(FLOCK_IES)));

  surv_ie_reset_signatures();
}

TEST_CASE("signature with more tokens than allowed is rejected",
          "[surv][ie]") {
  surv_ie_reset_signatures();
  surv_ie_tok_t too_many[SURV_IE_MAX_TOKS + 1];
  memset(too_many, 0, sizeof(too_many));
  TEST_ASSERT_FALSE(surv_ie_add_signature(too_many, SURV_IE_MAX_TOKS + 1));
  TEST_ASSERT_FALSE(surv_ie_add_signature(NULL, 3));
  TEST_ASSERT_FALSE(surv_ie_add_signature(too_many, 0));
}

TEST_CASE("overlay cap of 8 signatures is respected", "[surv][ie]") {
  surv_ie_reset_signatures();
  const surv_ie_tok_t t1[] = {{45, 0, {0}}};
  for (int i = 0; i < SURV_IE_MAX_SIGS; i++) {
    TEST_ASSERT_TRUE(surv_ie_add_signature(t1, 1));
  }
  TEST_ASSERT_FALSE(surv_ie_add_signature(t1, 1));
  surv_ie_reset_signatures();
}
```

Add `test_surv_ie.c` to the `TESTS` variable in `test/Makefile`.

- [ ] **Step 2: Run and verify it fails**

Run: `make -C firmware/components/surveillance_detect/test run-tests`. Expected: `surv_ie.h` does not exist.

- [ ] **Step 3: Implement the pattern and binary comparison**

`include/surv_ie.h`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Probe request Information Element fingerprint.
//
// Provenance: the signature and corrupt-frame handling come from
// colonelpanichacks/flock-you (MIT), which in turn is based on research by
// DeFlockJoplin and Pintor & Atzori, "Analysis of Wi-Fi Probe Requests
// Towards Information Element Fingerprinting", GLOBECOM 2022.
//
// Difference from the original: there, the signature is built as a string with
// snprintf and compared with strcmp INSIDE the promiscuous callback. Here it is
// compared token by token in binary, with no string formatting in the hot path.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SURV_IE_VENDOR_TAG 221
#define SURV_IE_MAX_TOKS   16
#define SURV_IE_MAX_SIGS   8

typedef struct {
  uint8_t tag;
  uint8_t vlen;       // 0 if not vendor
  uint8_t vendor[7];  // first 7 bytes of vendor payload
} surv_ie_tok_t;

// 1 = SSID IE present with length 0; 0 = present with non-zero length; -1 = absent.
int surv_ie_is_wildcard_probe(const uint8_t* ies, int len);

// true if the IEs reproduce the primary Flock signature.
bool surv_ie_matches_flock(const uint8_t* ies, int len);
```

`surv_ie.c` - **the code in this step was corrected after the plan was written and the real implementation is authoritative**; see `firmware/components/surveillance_detect/surv_ie.c`. What the original code in this document got wrong, and what MUST NOT be lost again:

1. **The three field patches were missing.** The plan promised them in prose and failed to implement them: *phantom overflow* skipping, TLV resynchronization, and canonicalization anchored to the LiteON vendor element. Without them, the comparator is stricter than the reference implementation and **silently fails to recognize real cameras**: a differential test across 25,000 inputs failed 3,020 times, all in the direction of failing to detect.
2. **Vendor tokens are up to 8 bytes, not 7.** The reference encodes `min(elen, 8)`, and `vlen` must be compared: two IEs sharing the same first 7 bytes but having different lengths are NOT the same token.
3. **Retrying without FCS is necessary.** It was previously removed because it looked like dead code under the old semantics; under the rule "the signature ends where the list ends", a trailing FCS appends tokens and breaks the match, making the retry necessary again.
4. **The original canonicalization is inconsistent and is reproduced intentionally.** It only canonicalizes if the string does not already start with `2,12,127,`, so an extra tag inside the prefix breaks it while one before the prefix does not. That is the road-validated behavior; deviating from it makes us more permissive.

The guarantee that the binary reimplementation preserves the original semantics is the differential test in `test/test_surv_ie_diff.c`, which runs both algorithms across 228,000 inputs and requires total agreement. That test is the acceptance criterion for any future change to the comparator.

- [ ] **Step 4: Run and verify they pass**

Run: `make -C firmware/components/surveillance_detect/test run-tests`.
Expected: all eleven tests in `[surv][ie]` pass.

- [ ] **Step 5: Record the real-vector coverage note**

Append to the end of `test_surv_ie.c`:

```c
// PENDING REAL CAPTURE: the tests above use a synthetic vector.
// The "phantom overflow" and TLV resync cases that flock-you encountered in
// the field cannot be reproduced without real frames. Once Task 15 produces
// evidence pcaps, extract the IEs from a real tier-4 frame and add it here as
// FLOCK_IES_REAL, with its own TEST_CASE.
```

This is not an implementation placeholder: the function is complete and tested. It is a note tracking coverage that depends on captures not yet recorded.

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add binary IE fingerprint matcher for Flock probes"
```

---

## Task 7: Scoring, Tiers, and Dedupe Engine

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_engine.h`
- Modify: `firmware/components/surveillance_detect/surv_engine.c`
- Test: `firmware/components/surveillance_detect/test/test_surv_engine.c`

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
  // Persistence tracker (SURV_CLASS_PERSIST, from eye-spy): an unknown MAC
  // seen >=3 times across >=5 min is a potential follower.
  // Returns true on the sighting that triggers scoring.
  bool    surv_engine_note_unknown(const uint8_t mac[6], uint32_t now_ms);
  ```

Time is passed in as a `now_ms` parameter rather than read from `esp_timer`: this makes it possible to test decay and cooldown on the host without waiting real minutes.

- [ ] **Step 1: Write the failing tests**

`test/test_surv_engine.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>
#include "surv_engine.h"
#include "surv_test.h"

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

TEST_CASE("a detection adds its points to the score", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
}

TEST_CASE("score decays by 1 point every 60 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t ev = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&ev, 5, 1000);
  surv_engine_tick(1000 + 60000);
  TEST_ASSERT_EQUAL_UINT8(4, surv_engine_score());
  surv_engine_tick(1000 + 120000);
  TEST_ASSERT_EQUAL_UINT8(3, surv_engine_score());
}

TEST_CASE("score never drops below zero", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_tick(600000);
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("same class does not re-score before 120 s", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t a = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t b = mk(0x02, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&a, 5, 1000);
  surv_engine_submit(&b, 5, 30000);  // different MAC, same class, within 120 s
  TEST_ASSERT_EQUAL_UINT8(5, surv_engine_score());
  surv_engine_submit(&b, 5, 1000 + 120001);
  TEST_ASSERT_EQUAL_UINT8(10, surv_engine_score());
}

TEST_CASE("a high tier is not degraded by a lower one", "[surv][engine]") {
  surv_engine_reset();
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_engine_submit(&hi, 5, 1000);
  surv_engine_submit(&lo, 5, 2000);
  TEST_ASSERT_EQUAL_UINT8(SURV_TIER_IE_SIG, surv_engine_best_tier(hi.mac));
}

TEST_CASE("5 s dedupe suppresses repeats at the same tier",
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

TEST_CASE("a higher tier preempts dedupe", "[surv][engine]") {
  surv_engine_reset();
  surv_engine_register_emit_cb(on_emit);
  s_emits = 0;
  surv_event_t lo = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_ADDR13);
  surv_event_t hi = mk(0x01, SURV_CLASS_FLOCK, SURV_TIER_IE_SIG);
  surv_engine_submit(&lo, 5, 1000);
  surv_engine_submit(&hi, 5, 1500);  // within 5 s, but higher tier
  TEST_ASSERT_EQUAL_INT(2, s_emits);
}

TEST_CASE("unknown MAC seen 3 times over 5 min scores persistence",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x01};
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 1000));
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 120000));
  // third sighting, and more than 5 min have elapsed since the first
  TEST_ASSERT_TRUE(surv_engine_note_unknown(mac, 1000 + 300001));
  TEST_ASSERT_EQUAL_UINT8(2, surv_engine_score());
}

TEST_CASE("unknown MAC seen 3 times within 1 min does not score",
          "[surv][engine]") {
  surv_engine_reset();
  const uint8_t mac[6] = {0xde, 0xad, 0x00, 0x00, 0x00, 0x02};
  surv_engine_note_unknown(mac, 1000);
  surv_engine_note_unknown(mac, 20000);
  TEST_ASSERT_FALSE(surv_engine_note_unknown(mac, 40000));
  TEST_ASSERT_EQUAL_UINT8(0, surv_engine_score());
}

TEST_CASE("device table saturates at 200 MACs without overflowing", "[surv][engine]") {
  surv_engine_reset();
  for (int i = 0; i < 300; i++) {
    surv_event_t ev = mk((uint8_t) (i & 0xff), SURV_CLASS_CAM, SURV_TIER_ADDR2);
    ev.mac[4] = (uint8_t) (i >> 8);
    surv_engine_submit(&ev, 3, (uint32_t) (1000 + i * 10));
  }
  TEST_PASS();  // must not crash
}
```

- [ ] **Step 2: Run and verify it fails**

Run: `make -C firmware/components/surveillance_detect/test run-tests`. Expected: `surv_engine.h` does not exist.

- [ ] **Step 3: Implement the engine**

`include/surv_engine.h` with the signatures from the Interfaces block, and `surv_engine.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Confidence engine. Combines two models:
//   - per-MAC tier, from flock-you (MIT): how confident am I about this device.
//   - global score with decay, from eye-spy (Apache-2.0): how surveilled is
//     this location.
// They are independent axes: an SSID "flock" is tier 0 and simultaneously 5 points.
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
    return NULL;  // table full: keep scoring, stop tracking the device
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

  // Global score: cooldown per class, not per MAC.
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

  // Emission: 5 s dedupe per MAC, preempted by a higher tier.
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

// --- Persistence tracker (from eye-spy, Apache-2.0) -------------------------
// A MAC that matches no known signature but reappears over time is the
// signal of a follower. Purged after 30 min of absence.
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

`surv_engine_reset()` must also clear `s_track` with `memset(s_track, 0, sizeof(s_track));`.

**File ordering:** the declarations of `surv_track_t` and `s_track` must appear **before** `surv_engine_reset()`, even though they appear afterward in this document for exposition clarity. If transcribed in the document order, `surv_engine_reset()` will reference an undeclared identifier and fail to compile.

- [ ] **Step 4: Run and verify they pass**

Run: `make -C firmware/components/surveillance_detect/test run-tests`.
Expected: all tests in `[surv][engine]` pass.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add scoring engine with decay, cooldown and tier dedupe"
```

---

## Task 8: Signature Overlay Parser

**Files:**
- Create: `firmware/components/surveillance_detect/include/surv_overlay.h`
- Modify: `firmware/components/surveillance_detect/surv_overlay.c`
- Test: `firmware/components/surveillance_detect/test/test_surv_overlay.c`

**Interfaces:**
- Consumes: `surv_oui_entry_t`, `surv_class_t`, `surv_kw_entry_t`, `surv_uuid_entry_t`, `surv_ie_tok_t`, `surv_ie_add_signature()`, `surv_ie_reset_signatures()`, `surv_signatures_build_effective*()`.
- Produces:
  ```c
  typedef struct { uint16_t added; uint16_t removed; uint16_t skipped; } surv_overlay_stats_t;
  void surv_overlay_reset(void);
  surv_overlay_stats_t surv_overlay_parse(const char* text);
  ```

The parser accepts **in-memory text**, not a file path. Reading the file is the app's responsibility (Task 17). That keeps the parser 100% testable on the host.

- [ ] **Step 1: Write the failing tests**

`test/test_surv_overlay.c`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_ie.h"
#include "surv_match.h"
#include "surv_overlay.h"
#include "surv_test.h"

TEST_CASE("added OUI starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+oui,aa:bb:cc,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  surv_match_init();
  const uint8_t mac[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
  const surv_oui_entry_t* e = surv_match_oui(mac);
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_FLOCK, e->klass);
}

TEST_CASE("removed OUI stops matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("-oui,70:c9:4e\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.removed);
  surv_match_init();
  const uint8_t mac[6] = {0x70, 0xc9, 0x4e, 0x11, 0x22, 0x33};
  TEST_ASSERT_NULL(surv_match_oui(mac));
}

TEST_CASE("comments and empty lines are ignored", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("# comment\n\n   \n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(0, st.removed);
  TEST_ASSERT_EQUAL_UINT16(0, st.skipped);
}

TEST_CASE("malformed line is counted as skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(
      "+oui,notamac,flock,5,2\n"
      "+oui,aa:bb\n"
      "garbage\n"
      "+oui,dd:ee:ff,flock,5,2\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  TEST_ASSERT_EQUAL_UINT16(3, st.skipped);
}

TEST_CASE("null or empty text does not fail", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse(NULL);
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  st = surv_overlay_parse("");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
}

TEST_CASE("added SSID keyword starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+ssid,mycamera,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const surv_kw_entry_t* e = surv_match_ssid("net-MyCaMeRa-2");
  TEST_ASSERT_NOT_NULL(e);
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, e->klass);
}

TEST_CASE("added UUID starts matching", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+uuid,fd6f,cam,3\n");
  TEST_ASSERT_EQUAL_UINT16(1, st.added);
  const uint8_t adv[] = {0x03, 0x03, 0x6f, 0xfd};
  surv_ble_hit_t hits[SURV_BLE_MAX_HITS];
  TEST_ASSERT_EQUAL_UINT8(1, surv_match_ble_adv(adv, sizeof(adv), hits));
  TEST_ASSERT_EQUAL_INT(SURV_CLASS_CAM, hits[0].klass);
}

TEST_CASE("overlay IE signature is added to the compiled-in one",
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

TEST_CASE("malformed IE signature is skipped", "[surv][overlay]") {
  surv_overlay_reset();
  surv_overlay_stats_t st = surv_overlay_parse("+iesig,221:abc\n");
  TEST_ASSERT_EQUAL_UINT16(0, st.added);
  TEST_ASSERT_EQUAL_UINT16(1, st.skipped);
}

TEST_CASE("cap of 256 OUIs is respected", "[surv][overlay]") {
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

- [ ] **Step 2: Run and verify it fails**

Run: `make -C firmware/components/surveillance_detect/test run-tests`. Expected: `surv_overlay.h` does not exist.

- [ ] **Step 3: Restructure tables to support the overlay**

`surv_match_init()` stops reading the base table directly: it builds an effective table in RAM = base + added - removed. In `surv_signatures.c` add:

```c
#define SURV_OVERLAY_MAX_OUIS 256

// NOTE: size using sizeof(OUIS), NEVER a literal. The base table grew from 74
// to 75 when adding the Axon OUI, and a stale literal overflows the end of
// this array right when the overlay is full.
static surv_oui_entry_t
    s_effective[sizeof(OUIS) / sizeof(OUIS[0]) + SURV_OVERLAY_MAX_OUIS];
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

`surv_signatures_ouis()` and `surv_signatures_oui_count()` return the effective table if built, or the base table otherwise. The Task 1 test (75 OUIs) continues to pass because without an overlay the effective table is identical to the base table.

- [ ] **Step 4: Implement the parser**

`surv_overlay.c` - parses line by line with `strtok_r` over a bounded copy, without `malloc`:

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_overlay.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "surv_signatures.h"

// Overlay caps are defined ONCE in surv_signatures.h: there they size the
// effective arrays and here they limit how many entries are accepted.
// Duplicating them across both files is the same class of bug as the stale
// literal 74: if someone bumps one without the other, build_effective writes
// out of bounds and the compiler says nothing.
#define MAX_ADD      SURV_OVERLAY_MAX_OUIS
#define MAX_ADD_KW   SURV_OVERLAY_MAX_KWS
#define MAX_ADD_UUID SURV_OVERLAY_MAX_UUIDS
#define MAX_REMOVE   64
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
  if (s == NULL || *s == '\0') {
    return false;
  }
  char* end = NULL;
  long  tag = strtol(s, &end, 10);
  // Use strtol instead of atoi, and reject tag 0. atoi("xyz") silently returns
  // 0, and a token with tag 0 is indistinguishable from the SSID element:
  // tokens_match_sig skips it BEFORE comparing against the signature, so the
  // signature never completes and remains dead while occupying one of the 8
  // overlay slots. Same failure mode as token 221 without payload, and neither
  // gives any warning.
  if (end == s || (*end != '\0' && *end != ':') || tag < 1 || tag > 255) {
    return false;
  }
  const char* colon = strchr(s, ':');
  out->tag = (uint8_t) tag;
  if (colon == NULL) {
    // Reject a vendor token without payload. The matcher decides whether to
    // compare vendor bytes via `tag == 221`, never via vlen, so accepting a
    // bare "221" produces a signature that loads without complaint and then
    // NEVER matches, with no way to debug it. Counting the line as skipped is
    // visible to the user.
    return out->tag != SURV_IE_VENDOR_TAG;
  }
  const char* hex = colon + 1;
  size_t      hl = strlen(hex);
  if (hl != 14) {  // exactly 7 vendor bytes
    return false;
  }
  // (see above: a token 221 without payload is rejected before reaching here)
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
    return true;  // ignored, does not count as skipped
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
    // Copying into the pool is required: `value` points into the line buffer,
    // which is reused on the next iteration.
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
    // The rest of the line holds the tokens: "2", "12", "221:506f9a16030103"...
    // `value` is the first token; the rest come from strtok_r.
    surv_ie_tok_t toks[SURV_IE_MAX_TOKS];
    uint8_t       n = 0;
    const char*   tok = value;
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

`surv_signatures_build_effective_kws()` and `surv_signatures_build_effective_uuids()` are the counterparts of `surv_signatures_build_effective()` for keywords and UUIDs: they copy the base table into a static RAM array and append the overlay entries at the end. Without an overlay they return the base table, so the tests from Tasks 3 and 4 continue to pass.

- [ ] **Step 5: Run and verify they pass**

Run: `make -C firmware/components/surveillance_detect/test run-tests`.
Expected: all tests in `[surv][overlay]` pass and the Task 2 tests continue to pass (the effective table without an overlay is identical to the base table).

- [ ] **Step 6: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add microSD signature overlay parser"
```

---

## Task 9: Ring Buffer, Radio Callbacks, and Public API

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

- [ ] **Step 1: Add `RADIO_SELECT_SURVEILLANCE`**

In `radio_selector.h`, add to the enum:

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

In `radio_selector.c`:

```c
void radio_selector_set_surveillance(void) {
  radio_selected_option = RADIO_SELECT_SURVEILLANCE;
}
```

- [ ] **Step 2: Implement the ring buffer and promiscuous callback**

`surveillance_detect.c`. The callback only compares and enqueues; no `printf`, `malloc`, or disk writes:

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

// Points for the WiFi ODID engine. Same weight as the ODID entry in the BLE
// table (surv_signatures.c); named so the duplication is explicit.
#define SURV_PTS_ODID_WIFI 4

// The OUI entry `tier` is a CEILING, never the event tier. Written once: all
// three detection paths apply the same clamp, and reversing the ternary in any
// of them would promote a contract manufacturer OUI to a confirmatory tier.
static inline uint8_t clamp_tier(uint8_t computed, uint8_t ceiling) {
  return computed > ceiling ? ceiling : computed;
}

static void IRAM_ATTR wifi_sniffer_cb(void* buf,
                                      wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*) buf;
  // Discard frames flagged with hardware receive errors, matching ESP-IDF's
  // example sniffer. A corrupted frame can carry an altered addr2 that
  // matches a known OUI by chance, feeding garbage into the tier-1/tier-2
  // OUI-only paths (already the noisiest). Since the project's acceptance
  // criterion is ZERO false positives for class FLOCK/ALPR, this is mandatory.
  if (p->rx_ctrl.rx_state != 0) {
    return;
  }
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

  // Tier 3/4: probe request with wildcard SSID from a known OUI.
  if (is_mgmt && subtype == 4 && len > 24) {
    const surv_oui_entry_t* e = surv_match_oui(addr2);
    if (e != NULL) {
      const uint8_t* ies = pl + 24;
      int            ielen = len - 24;
      if (surv_ie_is_wildcard_probe(ies, ielen) == 1) {
        memcpy(ev.mac, addr2, 6);
        ev.klass = e->klass;
        ev.tier = clamp_tier(surv_ie_matches_flock(ies, ielen)
                                 ? SURV_TIER_IE_SIG
                                 : SURV_TIER_PROBE,
                             e->tier);
        queue_push(&ev, e->points);
        return;
      }
    }
  }

  // Drone Remote ID over WiFi: management frame addressed to the fixed ASTM
  // F3411 NaN MAC. Does not depend on any OUI. Engine from eye-spy (Apache-2.0).
  static const uint8_t ODID_NAN[6] = {0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00};
  if (is_mgmt && memcmp(addr1, ODID_NAN, 6) == 0) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = SURV_CLASS_ODID;
    ev.tier = SURV_TIER_ADDR2;
    queue_push(&ev, SURV_PTS_ODID_WIFI);
    return;
  }

  // Tier 2: OUI in addr2 of any frame.
  const surv_oui_entry_t* e2 = surv_match_oui(addr2);
  if (e2 != NULL) {
    memcpy(ev.mac, addr2, 6);
    ev.klass = e2->klass;
    ev.tier = clamp_tier(SURV_TIER_ADDR2, e2->tier);
    queue_push(&ev, e2->points);
    return;
  }

  // Tier 1: OUI in addr1 or addr3. Echoes, prone to false positives.
  const surv_oui_entry_t* e1 = surv_match_oui(addr1);
  if (e1 == NULL && is_mgmt) {
    e1 = surv_match_oui(addr3);
    addr1 = addr3;
  }
  if (e1 != NULL) {
    memcpy(ev.mac, addr1, 6);
    ev.klass = e1->klass;
    ev.tier = clamp_tier(SURV_TIER_ADDR13, e1->tier);
    queue_push(&ev, e1->points);
  }
}
```

- [ ] **Step 3: Implement the engine task**

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

`surv_begin()` calls `surv_match_init()`, `surv_engine_reset()`, `surv_engine_register_emit_cb(engine_emit)`, `radio_selector_set_surveillance()`, creates the task with `xTaskCreate(engine_task, "surv_engine", 4096, NULL, 5, &s_engine_task)`, and starts `surv_radio_start(profile, active_scan)` (Task 10).

- [ ] **Step 4: Compile for the real target**

```bash
cd firmware && idf.py build
```

Expected: build OK.

- [ ] **Step 5: Commit**

```bash
git add firmware/components/surveillance_detect firmware/components/radio_selector
git commit -m "feat(surveillance): add lock-free queue, sniffer callback and public API"
```

---

## Task 10: Radio Scheduler

**Files:**
- Modify: `firmware/components/surveillance_detect/surv_radio.c`
- Create: `firmware/components/surveillance_detect/include/surv_radio.h`

**Interfaces:**
- Consumes: `surv_profile_t`, `wifi_sniffer_cb`, `surv_match_ble_adv()`.
- Produces: `esp_err_t surv_radio_start(surv_profile_t p, bool active_scan)`, `void surv_radio_stop(void)`, `uint8_t surv_radio_current_channel(void)`.

- [ ] **Step 1: Implement channel hopping with region clamping**

```c
// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_radio.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SURV_DWELL_MS 250

static const uint8_t HOP_PRIMARY[]  = {11, 6, 1};
static const uint8_t HOP_EXTENDED[] = {13, 8, 3};

// Clamps to the configured region's nchan: with GLOBAL (Minino's default),
// channels are 1-11 and hopping to 13 hears nothing.
static bool channel_allowed(uint8_t ch) {
  wifi_country_t c;
  if (esp_wifi_get_country(&c) != ESP_OK) {
    return ch <= 11;
  }
  return ch >= c.schan && ch < (uint8_t) (c.schan + c.nchan);
}
```

The hopping task iterates over `HOP_PRIMARY` with a 250 ms dwell and, in the `SURV_PROFILE_SURVEIL` profile, interleaves `HOP_EXTENDED` every fourth pass, skipping channels rejected by `channel_allowed()`.

- [ ] **Step 2: Implement the phases**

Both stacks remain initialized; only scanning is mutually exclusive. Do **not** call `esp_bluedroid_deinit` every cycle: it fragments the heap and costs hundreds of ms.

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

`ble_window_ms()` calls `esp_ble_gap_start_scanning()` with `BLE_SCAN_TYPE_PASSIVE` and `window == interval`; when finished, it calls `esp_ble_gap_stop_scanning()`. `wifi_window_ms()` calls `esp_wifi_set_promiscuous(true)` with `wifi_sniffer_cb`, runs channel hopping, and finishes with `esp_wifi_set_promiscuous(false)`.

- [ ] **Step 3: Compile and verify on hardware**

```bash
cd firmware && idf.py build && idf.py -p /dev/ttyACM0 flash monitor
```

No app yet; invoke it from the console monitor or via a temporary call in `app_main`. Expected in the log: phase alternation and channel switches every 250 ms, with no `Guru Meditation` or watchdog triggers over 10 minutes.

- [ ] **Step 4: Commit**

```bash
git add firmware/components/surveillance_detect
git commit -m "feat(surveillance): add radio phase scheduler with region-aware hopping"
```

---

## Task 11: App, Menu Entry, and Screens

**Files:**
- Create: `firmware/main/apps/surveillance/surveillance_module.c`, `.h`
- Create: `firmware/main/apps/surveillance/surveillance_screens.c`, `.h`
- Modify: `firmware/main/modules/menus_module/menus_include/menus.h`

**Interfaces:**
- Consumes: `surv_begin()`, `surv_stop()`, `surv_register_cb()`, `surv_queue_overflows()`.
- Produces: `void surveillance_module_begin(void)` - used by `menus.h` as `on_enter_cb`.

- [ ] **Step 1: Add the menu entry**

In `menus.h`, add to the enum next to `MENU_GPIO_APPS`:

```c
  MENU_SURVEILLANCE,
```

and to the menus table, with `parent_idx = MENU_APPLICATIONS`:

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

plus `#include "surveillance_module.h"` in the includes section of `menus.h`.

- [ ] **Step 2: Define the status level thresholds in one place**

The thresholds are engine semantics, not screen semantics, and are consumed by both this task and Task 12 (LEDs). Writing them as literals in two places means that if someone adjusts one without the other, the screen and LEDs will disagree on the alert level, which is one of the most confusing possible failures in the field. Add to `include/surv_engine.h`:

```c
// Status level thresholds from the eye-spy scoring model (Apache-2.0):
// 0-2 CLEAR, 3-5 CAUTION, 6+ ALERT. Consumed by the screen and LEDs.
#define SURV_SCORE_CAUTION 3
#define SURV_SCORE_ALERT   6
```

- [ ] **Step 3: Implement the main screen**

`surveillance_screens.c`, using `oled_screen_display_text` across the 8 pages:

```c
void surveillance_screens_show_status(uint8_t score, const char* profile,
                                      const char* last_label, uint8_t last_tier,
                                      int8_t rssi, uint8_t channel) {
  char buf[24];
  oled_screen_clear_buffer();
  snprintf(buf, sizeof(buf), "SURVEIL [%s]", profile);
  oled_screen_display_text(buf, 0, 0, false);

  const char* level = score >= SURV_SCORE_ALERT
                          ? "ALERT"
                          : (score >= SURV_SCORE_CAUTION ? "CAUTION" : "CLEAR");
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

- [ ] **Step 4: Implement the help screen with the regional notice**

Spec section 9.4 requires this: with the base table in Mexico, the app can run for an hour without a single detection, and that is the expected result. Without stating this, the firmware looks broken.

```c
static const char* HELP_LINES[] = {
    "Passive cam &",
    "tracker detector.",
    "",
    "Flock/ALPR works",
    "in US & Canada.",
    "In MX it is normal",
    "to see nothing.",
    "",
    "Load regional sigs",
    "on microSD:",
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

Reached via BUTTON_UP from the main screen; BUTTON_LEFT returns.

- [ ] **Step 5: Implement lifecycle and button handling**

`surveillance_module.c`, following the pattern of `main/apps/wifi/deauth_detector/detector.c`:

Helpers used by the block below, in the same file:

```c
static surv_profile_t load_profile_from_prefs(void) {
  uint8_t v = preferences_get_uchar("surv_profile", SURV_PROFILE_SURVEIL);
  return (v <= SURV_PROFILE_TRACKERS) ? (surv_profile_t) v
                                      : SURV_PROFILE_SURVEIL;
}

static bool load_active_scan_from_prefs(void) {
  return preferences_get_uchar("surv_active", 0) != 0;
}

// Called from the engine task. Only saves state and marks the screen dirty:
// OLED refresh and microSD writes happen in the app loop, never here.
static void on_detection(const surv_event_t* ev, uint8_t score) {
  s_last_event = *ev;
  s_last_score = score;
  s_have_event = true;
  s_screen_dirty = true;
}
```

`surveillance_screens_show_radio_busy()` is a static screen in `surveillance_screens.c` displaying the text `802.15.4 active` / `Restart Minino`.

```c
void surveillance_module_begin(void) {
  if (radio_selector_is_stack_initialized()) {
    surveillance_screens_show_radio_busy();
    menus_module_set_app_state(true, surveillance_input_cb);
    return;  // 802.15.4 owns the antenna; do not contend for it
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

- [ ] **Step 6: Compile, flash, and verify on hardware**

```bash
cd firmware && idf.py build && idf.py -p /dev/ttyACM0 flash monitor
```

1. Menu -> Applications -> Surveillance: the app starts and displays `score 0 CLEAR`.
2. BUTTON_UP displays the help screen with the regional notice; BUTTON_LEFT returns.
3. BUTTON_LEFT from the main screen returns to the menu without hanging.
4. Re-enter and exit three times: free heap must not drop monotonically (verify with `esp_get_free_heap_size()` in the log).

- [ ] **Step 7: Commit**

```bash
git add firmware/main/apps/surveillance firmware/main/modules/menus_module
git commit -m "feat(surveillance): add app, menu entry and status screen"
```

---

## Task 12: Per-Tier Buzzer and LED Cadence

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`

**Interfaces:**
- Consumes: `buzzer_set_freq()`, `buzzer_play_for()`, `led_start_blink()`, `led_start_breath()`, `led_stop()`, `preferences_get_uchar()`.
- Produces: `void surveillance_alert(uint8_t tier, uint8_t score)`.

Minino does not have an RGB LED (`leds.h` exposes `LED_LEFT`/`LED_RIGHT` with brightness only, no color), so the status level is shown on screen and the LEDs encode the level via cadence.

- [ ] **Step 1: Implement per-tier tones**

```c
// Tones from flock-you (MIT): the detection method can be distinguished by ear
// while driving, without looking at the screen.
static void tier_chirp(uint8_t tier) {
  uint8_t mask = preferences_get_uchar("surv_beep", 0x1F);
  if ((mask & (1u << tier)) == 0) {
    return;  // muted: the detection is still logged
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

- [ ] **Step 2: Implement LED status levels**

```c
static void update_leds(uint8_t score) {
  static uint8_t last_level = 0xff;
  uint8_t level = score >= SURV_SCORE_ALERT
                      ? 2
                      : (score >= SURV_SCORE_CAUTION ? 1 : 0);
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
    // pulse_count 0 and time_out 0 = blink indefinitely until led_stop().
    led_start_blink(LED_LEFT, 100, 0, 200, 200, 0);
    led_start_blink(LED_RIGHT, 100, 0, 200, 200, 0);
  }
}
```

The signature of `led_start_blink` matches `firmware/components/leds/include/leds.h`: six parameters, not two. Confirm the meaning of `duty` and `time_out` in `leds.c` before finalizing the values: if `pulse_count = 0` does not mean "indefinite", use a high count and re-arm blinking from `update_leds()`.

- [ ] **Step 3: Verify on hardware**

Flash and, with the microSD overlay targeting a personal device (Task 17) or by forcing a test detection:

1. Tier 4 sounds distinct from tier 1 and can be told apart by ear.
2. With `surv_beep` = 0 in NVS, nothing sounds while detections continue to be logged.
3. LEDs transition from off to breathing to blinking as the score rises.

- [ ] **Step 4: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): add per-tier buzzer tones and LED cadence levels"
```

---

## Task 13: CSV Logging with GPS

**Files:**
- Create: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`

**Interfaces:**
- Consumes: `sd_card_append_to_file()`, `gps_module_get_instance()`, `get_full_date_time()`, `surv_event_t`.
- Produces: `void surveillance_log_begin(void)`, `void surveillance_log_detection(const surv_event_t* ev, uint8_t score, gps_t* gps)`, `void surveillance_log_flush(void)`.

- [ ] **Step 1: Define the format**

In `surveillance_log.h`. The 14 WigleWifi columns come first, followed by the four custom columns at the end, so `cut -d, -f1-14` produces a file ready for WiGLE upload:

```c
#define SURV_DIR_NAME    "surveil"
#define SURV_CSV_LINE    200   // does not fit in the 150-byte CSV_LINE_SIZE
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

- [ ] **Step 2: Write the formatting helpers**

All in `surveillance_log.c`, using static buffers: no memory is allocated per detection.

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

// Wardriving date format. Without a GPS fix there is no reliable clock: write
// uptime in ms, which at least orders detections relative to each other.
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

// Accumulates in RAM and flushes in batches. Writing on every detection would
// wear out the microSD card and stall ring buffer draining.
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

`surveillance_log_flush()` performs the final `sd_card_append_to_file(s_csv_name, s_buf)` when exiting the app.

- [ ] **Step 3: Implement logging without a GPS fix**

```c
void surveillance_log_detection(const surv_event_t* ev, uint8_t score,
                                gps_t* gps) {
  char line[SURV_CSV_LINE];
  char lat[16] = "";
  char lon[16] = "";
  // Without a fix, log anyway with empty coordinates: losing a confirmed
  // camera because GPS has not fixed yet would be the worst possible tradeoff.
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

- [ ] **Step 4: Verify on hardware**

1. With a microSD card and a GPS fix: walk a block with the overlay targeting a personal device.
2. Remove the card and verify that `surveil/*.csv` has the proper header and rows with coordinates.
3. Run `cut -d, -f1-14 surveil/xxx.csv > wigle.csv` and verify that the columns match a `warfi/` CSV.
4. Repeat without a GPS fix: rows must appear with empty lat/lon fields rather than being dropped.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): log detections to WiGLE-compatible CSV with GPS"
```

---

## Task 14: Tier-4 GPX Export

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`

**Interfaces:**
- Consumes: Task 13 interfaces.
- Produces: `void surveillance_log_gpx_waypoint(const surv_event_t* ev, gps_t* gps)`.

DeFlock does **not** consume CSV: it is OpenStreetMap with tagged nodes. The GPX file is loaded as a layer in the iD editor to place nodes without repeating the drive.

- [ ] **Step 1: Implement waypoint logging**

Only for tier 4 detections with valid coordinates:

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

The file is opened with the GPX header in `surveillance_log_begin()` and closed with `</gpx>` in `surveillance_log_flush()`.

- [ ] **Step 2: Verify**

Generate a GPX file containing at least one waypoint and open it in [openstreetmap.org/edit](https://www.openstreetmap.org/edit) as a GPX layer. Expected: the waypoint appears at the proper location.

- [ ] **Step 3: Commit**

```bash
git add firmware/main/apps/surveillance
git commit -m "feat(surveillance): export tier-4 detections as GPX waypoints"
```

---

## Task 15: PCAP Evidence

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_log.c`, `.h`
- Modify: `firmware/components/surveillance_detect/surveillance_detect.c`, `include/surveillance_detect.h`

**Interfaces:**
- Consumes: `espressif__pcap` (already in `managed_components`, used by `wifi_sniffer`).
- Produces: the enqueued event carries a copy of the frame when `tier >= 3`.

Neither flock-you nor eye-spy saves raw evidence. This turns Minino into a research tool: the IE signature can be verified manually in Wireshark rather than taking the firmware's word for it. These same dumps feed the pending vectors in Task 6.

- [ ] **Step 1: Expand the queue item for tiers 3 and 4**

```c
#define SURV_EVIDENCE_MAX 320

typedef struct {
  surv_event_t ev;
  uint8_t      points;
  uint16_t     raw_len;                    // 0 if not copied
  uint8_t      raw[SURV_EVIDENCE_MAX];
} surv_qitem_t;
```

Copying only happens after a match has already triggered with `tier >= SURV_TIER_PROBE`, so there is no overhead on the common frame path.

- [ ] **Step 2: Open the pcap session**

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
  // 802.11 without radio header: matches what promiscuous mode delivers.
  return pcap_write_header(s_pcap, PCAP_LINK_TYPE_802_11);
}
```

Compare with `firmware/components/wifi_sniffer/cmd_pcap.c`, which already performs this sequence, and reuse its field names if they differ from those above.

- [ ] **Step 3: Write pcap with a 4 MB cap**

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
    // Stop writing evidence, but detection and CSV logging continue.
    surveillance_screens_show_pcap_full();
    return;
  }
  int64_t us = esp_timer_get_time();
  pcap_capture_packet(s_pcap, (void*) raw, len, (uint32_t) (us / 1000000),
                      (uint32_t) (us % 1000000));
  s_pcap_bytes += len + 16;
}
```

When exiting the app: call `pcap_del_session(s_pcap)` and `fclose(s_pcap_fp)`, in that order.

- [ ] **Step 4: Verify on hardware**

1. Trigger a tier 3 or 4 detection using the second board (Task 17).
2. Open `surveil/evidence_*.pcap` in Wireshark.
3. Verify that the frame is a Probe Request, that the SSID IE has length 0, and that the vendor IEs contain `50:6f:9a:16:03:01:03`.
4. Extract those IEs and add them as `FLOCK_IES_REAL` to the Task 6 test.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance firmware/components/surveillance_detect
git commit -m "feat(surveillance): dump tier-3/4 frames to pcap evidence file"
```

---

## Task 16: Settings - Profile, Active Scan, and Buzzer Mask

**Files:**
- Modify: `firmware/main/modules/settings/` (new submenu)
- Modify: `firmware/main/modules/menus_module/menus_include/menus.h`

**Interfaces:**
- Consumes: `preferences_put_uchar()`, `preferences_get_uchar()`.
- Produces: NVS keys `surv_profile` (0-2), `surv_active` (0/1), `surv_beep` (5-bit mask, default `0x1F`).

- [ ] **Step 1: Add the three options**

Following the pattern in `main/modules/settings/wifi/`, using `general_radio_selection` for the profile:

- **Profile**: `Flock/ALPR`, `Surveillance`, `Trackers`. Default `Surveillance`.
- **Active scan**: on/off, default off. Only visible in the `Flock/ALPR` and `Surveillance` profiles; ignored in `Trackers`.
- **Per-tier buzzer**: five toggles, one per tier, all enabled by default.

- [ ] **Step 2: Display the active scan indicator**

When `surv_active` is 1, the main screen displays `!` next to the profile name: enabling active scan causes the device to transmit probe requests and stop being undetectable, which must be visible without entering settings.

- [ ] **Step 3: Verify persistence**

1. Change the profile to `Flock/ALPR`, exit, and reboot the board.
2. Enter the app: it must start in `Flock/ALPR`.
3. Mute tier 1, reboot, and verify that tier 1 stays muted while tier 4 still sounds.

- [ ] **Step 4: Commit**

```bash
git add firmware/main/modules/settings firmware/main/modules/menus_module
git commit -m "feat(surveillance): add radio profile, active scan and beep mask settings"
```

---

## Task 17: microSD Overlay Loading and Field Validation

**Files:**
- Modify: `firmware/main/apps/surveillance/surveillance_module.c`
- Create: `docs/surveillance-signatures.md`

**Interfaces:**
- Consumes: `sd_card_read_file()`, `surv_overlay_parse()`, `surv_overlay_reset()`.
- Produces: nothing new; closes the loop.

- [ ] **Step 1: Read the file in `surveillance_module_begin()`**

Before `surv_begin()`, never from a callback:

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

The startup screen displays `sigs: 74 base +12 SD` or `SD: none`.

- [ ] **Step 2: Write user documentation**

`docs/surveillance-signatures.md` covering the file format (the five entry types: `oui`, `ssid`, `blename`, `uuid`, `iesig`), a complete example, the caps (256 OUIs, 64 keywords, 16 UUIDs, 8 IE signatures), and **the regional notice**, matching the help screen from Task 11: Flock Safety is deployed in the United States and, to a lesser extent, Canada; with the base table in Mexico, the app can run for an hour without a single detection and that is the expected result, not a bug.

Include instructions for uploading the file over WiFi using the `web_file_browser` that Minino already provides, without removing the microSD card.

- [ ] **Step 3: Two-board field validation**

Board A (transmitter): firmware with a test mode that transmits via `esp_wifi_80211_tx` a probe request with a length-0 SSID IE, `addr2` set to an OUI from the table, and the `FLOCK_IES` information elements. Reuse the pattern from `main/apps/wifi/ssid_spam` and `components/drone_id/spoofer`.

Board B (detector): the surveillance app running in the `Flock/ALPR` profile.

Do not rely on a single board receiving its own transmissions: the radio is half-duplex and self-reception is not supported.

- [ ] **Step 4: Verify the spec acceptance criteria**

| Criterion | How It Is Measured | Threshold |
|---|---|---|
| False positives, base table, 30 min in an urban area in Mexico | App in `Surveillance` profile, count FLOCK/ALPR class rows in the CSV | 0 |
| Detection of synthetic tier-4 frame with active channel hopping | Board A transmitting every second; stopwatch from Board B startup | < 2 s |
| Ring buffer overflow in 1 h | `surv_queue_overflows()` on the debug screen | 0 |
| Ring buffer drain stall caused by microSD writes | `esp_timer` log around `sd_card_append_to_file` | < 50 ms |
| Trackers app after refactor | Manual verification from Task 5 | No regression |

Any failing criterion returns to the task that caused it. A false-positive failure almost always points to an overly generic SSID keyword in Task 3.

- [ ] **Step 5: Commit**

```bash
git add firmware/main/apps/surveillance docs/surveillance-signatures.md
git commit -m "feat(surveillance): load signature overlay from microSD and document format"
```
