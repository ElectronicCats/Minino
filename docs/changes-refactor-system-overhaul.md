# Changes and Additions in `refactor/system-overhaul`

Reference document explaining **what changed**, **why**, and **how each component works**
in the `refactor/system-overhaul` branch compared to `main`.

- **Base:** `main` (`3eac1955`)
- **Branch:** `refactor/system-overhaul` (`a787b665`)
- **Scope:** 123 commits, 404 files, **+26,867 / -3,676** lines
- **Platform:** ESP32-C6 (`esp32c6`), ESP-IDF v5.5.1
- **Date written:** 2026-10-21
- **Companion documents:** [`esp-sdr-integration.md`](./esp-sdr-integration.md) (detailed
  SDR technical reference), [`rf-spectrum-advanced.md`](./rf-spectrum-advanced.md) (RF
  spectrum analyzer)

> **How to read this document.** Section 3 indexes every area by commit.
> Sections 4 through 20 explain one area each, starting with the "why" and
> following with the "how it works", including diagrams and tables. Section 21
> documents the **two uncommitted changes** present in the working tree at the
> time of writing. Section 22 covers the **regressions detected** when running
> the test suite, which should be reviewed before merging.
>
> **Phase 1 Update (2026-10-22):** `event_bus` was removed; WiFi and BLE are
> mutually exclusive in `radio_lease`. Sections 5 and 6 describe the previous
> state and are preserved for historical context; the current implementation
> lives in `radio_lease.c`, `wifi_service.c`, and the direct callbacks of each
> app.
>
> **Phase 2 Update (in progress):** `minino_app_t` and `app_manager` define the
> lifecycle contract; `rf_spectrum` and `esp_sdr` now exit cleanly via
> `app_manager_exit()` without rebooting. The remaining apps in the catalog
> still use `menus_module_restart()`.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Core Concept: From "Standalone Apps" to "A System with Explicit Resource Ownership"](#2-core-concept-from-standalone-apps-to-a-system-with-explicit-resource-ownership)
3. [Commit Map by Area](#3-commit-map-by-area)
4. [Native SDR: App, Driver, and Capture Bank](#4-native-sdr)
5. [`radio_lease`: Owner of the 2.4 GHz Radio](#5-radio_lease-owner-of-the-24-ghz-radio)
6. [`event_bus`: Typed Event Bus](#6-event_bus-typed-event-bus)
7. [`gap_dispatcher` and `gatt_dispatcher`: Bluedroid Fan-Out](#7-gap_dispatcher-and-gatt_dispatcher-bluedroid-fan-out)
8. [`wifi_service`: A Single `esp_wifi_init()`](#8-wifi_service-a-single-esp_wifi_init)
9. [Module Lifecycle Hardening](#9-module-lifecycle-hardening)
10. [State Privatization and Moving Data Out of Headers](#10-state-privatization)
11. [UI: Menu Catalog, Views, Icons, and OLED](#11-ui-menu-catalog-views-icons-and-oled)
12. [New App: RF Spectrum Analyzer](#12-new-app-rf-spectrum-analyzer)
13. [Buttons: Dedicated Queue and Task](#13-buttons-dedicated-queue-and-task)
14. [microSD: Mounting, Slots, and Accurate Error Reporting](#14-microsd-mounting-slots-and-accurate-error-reporting)
15. [BLE: HID, GATT, Spam, and GAP Synchronization](#15-ble-hid-gatt-spam-and-gap-synchronization)
16. [Build System: Variants, Kconfig, and Capture Bank Cap](#16-build-system-variants-kconfig-and-capture-bank-cap)
17. [Host Tests and CI](#17-host-tests-and-ci)
18. [Host Tools](#18-host-tools)
19. [Minor Changes by Module](#19-minor-changes-by-module)
20. [Complete Commit Table](#20-complete-commit-table)
21. [Uncommitted Changes in the Working Tree](#21-uncommitted-changes)
22. [Detected Regressions: Actual State of the Test Suite](#22-detected-regressions-actual-state-of-the-test-suite)
23. [Connecting the SDR to the ESPARGOS Web Client](#23-connecting-the-sdr-to-the-espargos-web-client)

---

## 1. Executive Summary

This branch accomplishes four major goals simultaneously, which are worth
separating because they have different motivations and risk profiles.

### 1.1 The SDR Moves from Dual-Boot to a Native App

Up to `main`, `Applications > SDR` handed boot over to a second image in
`ota_1`, and the device ceased running Minino. In this branch, the SDR is
**just another app in the firmware**: `esp_sdr_module.c` initializes the C6
capture engine in-process, captures I/Q bursts, and saves them to the microSD
card without touching `ota_1` and without turning off the screen.

The trade-off is that the C6 capture engine requires **SRAM block 2**
(`0x40840000`) as its capture bank, so the entire firmware DRAM image must fit
below that address. This required trimming 119 KB from the static image (see
section 4.5) and adding a linker script that turns any overlap into a link-time
error instead of a runtime memory corruption.

### 1.2 The Radio Is No Longer an Unowned Global Resource

ESP-IDF maintains **a single** promiscuous mode callback and **a single** GATTC
callback slot. Six subsystems competed for them: whichever registered last
overwrote the callback, leaving the others silently deaf.

- `radio_lease` (section 5) arbitrates promiscuous mode and the BLE stack using
  atomic reference counting.
- `gap_dispatcher` / `gatt_dispatcher` (section 7) provide event fan-out: one
  callback registered with Bluedroid, N subscribers underneath.
- `event_bus` (section 6) provides a typed event bus for non-GAP notifications.
- `wifi_service` (section 8) centralizes `esp_wifi_init()`.

### 1.3 Modules Stop Leaking Resources on Exit

A systematic hardening campaign (section 9) audited roughly 30 modules for a
recurring flaw: a `begin()` function that creates tasks, queues, mutexes, and
radio claims, paired with an `exit()` function that fails to release them. The
typical symptom was always the same: after exiting an app, the radio stayed
on, BLE scanning stopped working, or the `esp_timer` task was suspended and
froze the entire UI.

### 1.4 Host-Side Verification

16 new host test suites (section 17) built on Unity: 86+ test files compiling
natively under `gcc` on the PC against mocked ESP-IDF headers. This turns
"fixed by inspection" into "fixed and guarded by an automated test".

### 1.5 Branch Metrics

| Metric | Value |
|---|---|
| Commits | 123 |
| Files touched | 404 (206 new, 198 modified, 0 deleted) |
| Lines | +26,867 / -3,676 |
| New host test suites | 16 (15 in C + 1 in Python) |
| New components | `radio_lease`, `event_bus`, `wifi_service`, `gap_dispatcher` |
| New apps | `rf_spectrum` (RF spectrum analyzer) |
| New Kconfig options | `SDR_ENABLE`, `MININO_ZIGBEE_STACK`, `MININO_OPENTHREAD_STACK` |
| New console commands | `sdr_capture`, `sdtest`, `display`, `press`, `exit_app`, `threshold`, `spectrum_*` (6) |
| Host tests written | 278 (252 passing, 4 failing, 22 failing to compile prior to section 22 fixes) |
| DRAM image (IRAM+data+bss) | 374 KB -> 255 KB (budget: 256 KB) |
| Event bus static SRAM | 21,248 B -> 2,928 B (-17.9 KB) |
| Capture bank reserved from heap | 128 KB -> 64 KB (later restored to 128 KB; see section 23.6) |

---

## 2. Core Concept: From "Standalone Apps" to "A System with Explicit Resource Ownership"

Prior to this branch, the firmware consisted of independent apps calling global
ESP-IDF APIs directly. The failure pattern was always the same:

```
app A calls esp_wifi_set_promiscuous(true)         <- claims the sole callback
app B calls esp_wifi_set_promiscuous_rx_cb(cb_B)   <- steals the callback from A
app A finishes and calls esp_wifi_set_promiscuous(false) <- kills B's capture
```

And the exact same problem occurred with the BLE stack:

```
app C calls esp_bluedroid_deinit()   <- because its scan window ended
app D (which was actively running) loses the radio mid-operation
```

This branch introduces **explicit owners** for global hardware and stack
resources:

```
                     ┌──────────────────────────────┐
                     │        radio_lease            │
                     │   (owner of 2.4 GHz radio)   │
                     └──────────────────────────────┘
         ▲            ▲             ▲            ▲
         │            │             │            │
    ┌────┴───┐  ┌─────┴──┐   ┌──────┴───┐  ┌────┴─────┐
    │sniffer │  │deauth  │   │ spectrum │  │  SDR RX  │
    └────────┘  └────────┘   └──────────┘  └──────────┘
    (each claims a slot; the driver is only touched on 0->1 and 1->0 transitions)

                     ┌──────────────────────────────┐
                     │      gap_dispatcher          │
                     │   (GAP event fan-out hub)    │
                     └──────────────────────────────┘
         ▲            ▲             ▲            ▲
         │            │             │            │
    ┌────┴───┐  ┌─────┴──┐   ┌──────┴───┐  ┌────┴─────┐
    │bt_spam │  │HID     │   │bt_gattc  │  │ gattcmd  │
    └────────┘  └────────┘   └──────────┘  └──────────┘
```

For non-GAP notifications, the branch adds a typed event bus (`event_bus`) and
a singleton Wi-Fi initialization service (`wifi_service`).

Everything else in the branch (hardening, state privatization, host unit tests)
follows naturally: once every resource has a single authoritative owner,
"who releases what" is no longer ambiguous, and unit tests can verify the
contract directly.

---

## 3. Commit Map by Area

| Area | Commits |
|---|---|
| **Native SDR** (section 4) | `1df8bc9e` `fe1e0223` `f4f780e1` `09e3a77d` `0411c22c` `0813ff1c` `bce64f44` `10a808ab` `9e8c715e` `42bf30dd` `50496e10` `ff9877b8` `bf66e068` `510f4195` |
| **Capture bank and SRAM trimming** (sections 4.5, 16.4) | `ba55fa02` `46d235a1` `7a52b727` `b41d7803` `cf580c36` `e26111bd` `a3e1809c` `d5deb48d` |
| **`radio_lease`** (section 5) | `d56b9e7a` `6b313971` |
| **`event_bus`** (section 6) | `92bc14f9` `cca3cc43` `5508c825` |
| **`gap_dispatcher` / `gatt_dispatcher`** (section 7) | `3fbb6d9f` `574dfe5e` `029ee8c3` `2452a801` `de1e3dda` `37869cc6` |
| **`wifi_service`** (section 8) | `c2f47398` `fdf0f955` `d84eb372` `3321c30f` `f9aff880` `cdd1b586` `52138a31` `ca0a422d` |
| **Module hardening** (section 9) | `857a17a9` `6758054b` `7d4b08f2` `a3bf17f2` `e0b79f30` `b0163862` `00a2b83d` `339f0dbd` `69427bb5` `31953a42` `c69437cc` `1f86aba6` `f5c2747f` `e73754f7` `1a763db1` `931260ad` `093f257b` `2d97d08b` `f3c3c248` `d270b3a9` `6a078453` `6c958e7e` `280368ca` `913e2d81` `7ce2761d` `ddf071ca` |
| **State privatization** (section 10) | `080d8b29` `010e3446` `e2165751` `3d95ad2b` `80d47bbf` |
| **Moving data out of headers** (section 10.2) | `fd139212` `84879287` `d16af92c` `96901229` `56f58e1c` `f708fd5f` `7a9aaeff` |
| **UI: menus and views** (section 11) | `82927939` `8c485cb1` `7effae44` `f708fd5f` `05c33cbb` `689c925a` |
| **RF Spectrum Analyzer** (section 12) | `f85b1a53` `50496e10` `0411c22c` |
| **Buttons** (section 13) | `181feee8` |
| **microSD** (section 14) | `161643b6` `9cabe192` `c4f07def` `b8346169` |
| **BLE** (section 15) | `18871f04` `449b0144` `280368ca` `de1e3dda` `029ee8c3` `574dfe5e` `2452a801` `3fbb6d9f` |
| **Build system** (section 16) | `8d4173ce` `28cbc76d` `9c7599ee` `d861d64f` `ff3cd744` `c4eafa7f` `f67c001b` `045de824` `ad5c94ef` `7a9aaeff` |
| **Tests and CI** (section 17) | `88d13fb7` `da81e333` `689c925a` `c69437cc` `2c177f0d` `c6a814c4` `045de824` `47feae79` |
| **Host tools** (section 18) | `d861d64f` `ff3cd744` |

---

## 4. Native SDR

### 4.1 Overview

`Applications > SDR` opens a standard menu application in Minino:

```
firmware/main/modules/esp_sdr/
├── c6_sdr_rx.c        216 lines   capture engine driver (register-level)
├── c6_sdr_rx.h         67         driver API
├── esp_sdr_memory.c    57         capture bank heap reservation
├── esp_sdr_memory.h    41         capture bank base address and size
├── esp_sdr_module.c   192         menu application
├── esp_sdr_module.h    31
├── osmosdr_serial.c   446         esdr protocol parser (osmoSDR)
├── osmosdr_serial.h   115
├── sdr_screen.c        53         OLED view
├── sdr_screen.h        30
├── esdr_proto.h       132         protocol constants and structures
└── test/                          host test suite (20 tests)
```

> Note: `iq_pack.c/.h` do **not** live here. They reside in
> `firmware/components/vrt_packet/` (vendored from
> [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)) and **the firmware
> capture path does not call them**: raw hardware words are written directly to
> disk.

### 4.2 Application State

`esp_sdr_module.c:26-37` uses a single static struct, requiring no mutex or
background task:

```c
typedef struct {
  bool     running;
  uint32_t freq_mhz;
  uint32_t burst_count;
  uint32_t last_us;
  uint32_t last_samples;
} sdr_native_state_t;
```

The state machine is driven directly by button events:

```
         esp_sdr_module_begin()
                  │
                  ├── mounts the microSD card (BEFORE touching the radio)
                  ├── renders the idle screen
                  ├── menus_module_enable_input()
                  ├── menus_module_set_app_state(true, sdr_input_handler)
                  └── triggers the initial burst
                  │
    BUTTON_UP/DOWN │  BUTTON_RIGHT          │  BUTTON_LEFT
    tune +-5 MHz   │  capture + save        │  exit
         │         │                        │
    c6_sdr_rx_tune() sdr_burst_run()        │  menus_module_enable_input()
    2412..2472 MHz   │                     │  menus_module_set_app_state(false, NULL)
                      │                     │  c6_sdr_rx_stop()
                      │                     │  menus_module_return()
                      ▼                     ▼
               capture -> microSD       radio returned to Minino
```

### 4.3 The Capture Driver (`c6_sdr_rx.c`)

The C6 dump engine does not write into an arbitrary buffer: **it takes
ownership of an SRAM block** and dumps I/Q samples there at 80 MS/s. The
driver performs the following steps in order:

**a) Bring up the radio** (`c6_sdr_rx_init()`, lines 44-80)

```c
  if (!wifi_driver_get_initialized()) {
    if (!wifi_driver_init_sta()) {
      ESP_LOGE(TAG, "the Wi-Fi driver did not come up: stop the app that "
                   "holds the radio (BLE) and try again");
      return ESP_ERR_NO_MEM;
    }
  }
  radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
  s_saved_sram_owner = REG_READ(SRAM_OWNER_REG);
```

The capture engine **borrows the Wi-Fi PHY**: it needs its clocks and
calibration. Therefore, the Wi-Fi driver must be initialized. Promiscuous mode
was initially omitted here (`c6_sdr_rx.c:66-70`) to avoid allocating the
driver's promiscuous RX buffers (~37 KB of heap), though later testing (see
section 23.6) showed promiscuous mode is needed to keep the receiver active.

**b) Tune** (`c6_sdr_rx_tune()`, lines 82-118)

```c
  if (wifi_driver_get_channel() != ch) {
    wifi_driver_set_channel(ch);
  }
  pbus_workmode();
  rom_pbus_xpd_tx_off();
  ram_pbus_xpd_rx_on(1);
  set_rxclk_en(1);
```

Two important details:

- **Do not retune the radio if it is already on the target channel.** Calling
  `set_channel` unconditionally reinitializes the radio path, which allocates
  ~40 KB of heap buffers (commit `50496e10`).
- The RX chain is enabled via `pbus_workmode` -> `xpd_tx_off` ->
  `xpd_rx_on(1)` -> `set_rxclk_en(1)`, matching upstream
  (`esp-sdr-zephyr/lib/esp_sdr/src/esp_sdr_rx.c:78-98`).

**c) Capture** (`c6_sdr_rx_capture()`, lines 124-194)

```c
  /* canary: fill the entire buffer with a known pattern */
  for (uint32_t j = 0; j < num_samples; j++) s_iq_buf[j] = 0xa5a0055au;
  for (uint32_t j = 0; j < 4; j++)      s_iq_buf[num_samples + j] = 0x5a5aa5a5u ^ j;

  REG_WRITE(MODEM_CLK_REG1, 0xffffffffu);      /* force clocks to 80 MS/s */
  REG_WRITE(MODEM_CLK_REG2, 0x0007ffffu);
  REG_WRITE(MODEM_CLK_REG3, 0xffffffffu);
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(DUMP_CFG_REG1, ... | (1u<<6) | (2u<<12) | (3u<<18));  /* I/Q lanes */
  REG_WRITE(DUMP_CFG_REG2, ... | (15u << 15));                      /* source = raw RX I/Q */
  REG_CLR_BIT(DUMP_FE_REG, 1);

  /* hand SRAM bank over to the dump engine */
  REG_WRITE(SRAM_OWNER_REG, (owner & ~0xf00u) | 0x400u);   /* block 2 */
  __asm__ volatile("fence rw,rw" ::: "memory");

  /* trigger: clear DONE -> RUN -> pulse TRIGGER -> RUN */
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 18));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31) | (1u << 19));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31));

  /* wait: up to 25 ms */
  do { done = (REG_READ(DUMP_CTRL_REG) & (1u << 18)) != 0; }
  while (!done && esp_timer_get_time() < deadline);

  /* restore SRAM bank ownership to the CPU */
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(SRAM_OWNER_REG, owner);
```

**d) Verify**

Two distinct checks guard the capture:

| Check | What It Detects | Log Output |
|---|---|---|
| `!done` after 25 ms | Engine did not finish (forced clock failure, bank busy) | `Capture timeout (deadline exceeded)` |
| `s_iq_buf[0]` and `[1]` still contain the canary | Engine wrote nothing | `Capture incomplete (buffer unchanged)` |

The 4 **guard words** (`0x5a5aa5a5u ^ j`) are written *beyond* the requested
sample count, at indices 16380..16383 (the final 4 words of a 64 KiB bank).
That is why `C6_SDR_MAX_SAMPLES` is 16,380 rather than 16,384.

| Parameter | Value | Location |
|---|---|---|
| Minimum samples | 256 | `c6_sdr_rx.c:131` |
| Maximum samples | 16,380 (65,520 B) | `c6_sdr_rx.h:19` |
| Trigger timeout | 25,000 us | `c6_sdr_rx.c:165` |
| Sample rate | 80 MS/s | Fixed (later extended with decimation dividers in section 23.2) |
| Disk format | 32-bit raw words, interleaved I/Q | `esp_sdr_module.c:64-87` |

**e) Save to microSD** (`esp_sdr_module.c:64-87`)

```c
  char filename[64];
  files_ops_incremental_name_buf("/sdcard", "sdrcap", ".iq", filename, sizeof(filename));
  FILE* file = fopen(filename, "wb");
  const uint32_t* words = c6_sdr_rx_get_buffer();
  for (uint32_t i = 0; i < s_sdr.last_samples; i++) {
    if (fwrite(&words[i], sizeof(uint32_t), 1, file) != 1) { fclose(file); remove(filename); return false; }
  }
```

The filename is generated by `files_ops_incremental_name_buf()` using the
`"%s%02d"` format (`files_ops.c:320`), producing `/sdcard/sdrcap00.iq`,
`sdrcap01.iq`, ... (2 digits minimum, up to 9,999).

> **Correction to earlier `docs/esp-sdr-integration.md` drafts:** earlier notes
> stated `sdrcap_NNNN.iq` (4 digits). The code generates 2 digits.

### 4.4 The Capture Bank

This is the most critical piece of the architecture. The dump engine requires
SRAM block 2:

```
0x40800000 ┌────────────────────────────────┐
           │  blocks 0 and 1: DRAM image    │  <- firmware must fit here
           │  (IRAM + data + bss)           │
0x40840000 ├────────────────────────────────┤
           │                                │
           │  CAPTURE BANK (128 KiB)        │  <- dump engine writes here
           │  engine only touches the first │  <- 64 KiB or 128 KiB reserved (see 4.5 / 23.6)
           │  64 KiB: 16,380 words          │
0x40860000 └────────────────────────────────┘
```

**Two** mechanisms protect this region:

**1. The linker script** (`firmware/main/esp_sdr_bank.ld`, 30 lines)

```ld
MEMORY
{
  sram_seg (RWX) : org = 0x40800000, len = 0x40000
}

ASSERT(_heap_start <= 0x40840000, "SDR: the DRAM image overlaps the capture bank")
```

This redefines the `sram_seg` region (from ESP-IDF's `memory.ld`) with the same
origin and a length of `0x40000` (256 KiB), forcing every section placed in
`> sram_seg` to fit below `0x40840000`. If it overflows, linking fails with:

```
region 'sram_seg' overflowed by N bytes
```

and if `_heap_start` crosses into the bank, the explicit `ASSERT` triggers:

```
SDR: the DRAM image overlaps the capture bank
```

The script is appended via `-T` **after** the ESP-IDF linker scripts in
`firmware/main/CMakeLists.txt:87-92`:

```cmake
if(CONFIG_SDR_ENABLE)
    message(STATUS "SDR: capture bank cap ENABLED")
    idf_build_set_property(LINK_OPTIONS "-T" APPEND)
    idf_build_set_property(LINK_OPTIONS "${CMAKE_CURRENT_LIST_DIR}/esp_sdr_bank.ld" APPEND)
endif()
```

**2. Heap reservation** (`esp_sdr_memory.c:43-49`)

```c
#if defined(CONFIG_SDR_ENABLE)
SOC_RESERVE_MEMORY_REGION((intptr_t)SDR_CAPTURE_BANK_BASE,
                          (intptr_t)(SDR_CAPTURE_BANK_BASE + SDR_CAPTURE_BANK_SIZE),
                          sdr_capture_bank);
#endif
```

`SOC_RESERVE_MEMORY_REGION()` places the region descriptor into the
`.reserved_memory_address` section, which is read by the C6 heap port
(`components/heap/port/esp32c6/memory_layout.c`) to exclude the range from
heap allocation. Without this, `heap_caps` would hand out memory inside the
capture bank and the dump engine would overwrite live heap allocations.

### 4.5 64 KiB vs. 128 KiB Reservation, and Fitting the Firmware Below the Bank

The hardware bank is 128 KiB, though the dump engine **only writes the first
64 KiB**: a memory probe that fills the block with a test pattern and counts
modified words shows exactly 16,384 words written (the last at index 16,383).

In commit `9e8c715e` ("only the 64 KB the dump engine writes are taken off the
heap"), the reservation was reduced to 64 KiB so the upper 64 KiB returned to
the heap, expanding the largest contiguous heap block from 113 KB to 177 KB to
avoid `HCI_ERR_MEM_CAPACITY_EXCEEDED` during BLE scans:

```
heap_init: At 4083EE30 len 000011D0 (4 KiB): RAM     <- up to the bank
heap_init: At 40850000 len 0002C610 (177 KiB): RAM   <- above the 64 KiB bank
heap_init: At 4087C610 len 00002F54 (11 KiB): RAM
```

*(Note: As documented in sections 21.1 and 23.6, hardware testing with shorter
captures later proved that the full 128 KiB block must be reserved because
`SRAM_OWNER_REG` switches ownership of the entire 128 KiB block at once, and
BLE scanning still succeeds with the 128 KiB reservation in the SDR build
variant.)*

**The image budget.** With the unmodified firmware, `.bss` alone consumed
172 KiB, preventing the image from fitting below `0x40840000`. The static
memory reductions applied across the branch:

| Optimization | KB Saved |
|---|---|
| Moved Wi-Fi and PHY hot code out of IRAM | 45.6 |
| Moved OpenDroneID spoofers to the heap (allocated per configured drone) | 37.7 |
| Excluded `librftest.a` from linking (cert-test not needed without TX) | 44.7 |
| Reduced event bus subscriber table (16 -> 4 per topic) | 14.6 |
| Trimmed surveillance log, file tree, and SSID list buffers | 4.5 |
| Reduced surveillance detector tables (200 -> 80 devices, 50 -> 20 traces) | 7.0 |
| Moved OpenDroneID BLE drone state to the heap | 2.3 |
| Moved OLED font to flash | 1.0 |
| **Total** | **161.4** |

Result: IRAM+data+bss dropped from 374 KB to 255 KB, fitting within the 256 KB
budget.

### 4.6 The SDR Is Receive-Only for I/Q

The C6 **cannot transmit arbitrary I/Q**: its dump engine is a receive-only
unit (writing demodulated radio samples into SRAM) and the chip has no TX DAC
(unlike the ESP32-S3). The only test signal in ESP-IDF's RF test archive is the
cert-test CW tone, `esp_phy_wifi_tx_tone()`, in `librftest.a`.

Linking that tone function was tested in two ways (linking the full archive, or
linking only `rf_test.o` + `esp_phy_api.o` with a custom wrapper), but **it
does not radiate** without full cert-test initialization: HackRF measurements
showed an identical spectrum with the tone enabled and disabled. The cert-test
tone requires the full PHY cert-test state initialized by `rftest_init()` in
`wifi.o`, which pulls in `mac_common.o` along with 45 KiB of `.bss` (rxtest I/Q
buffers) that cannot fit below the capture bank.

Consequently, the `rf_tx` console command **was removed** (commit `bce64f44`),
along with `c6_sdr_tx.c`, `phy_tone_compat.c`, and the cert-test archive link
(`10a808ab`). The TX mode of the osmoSDR protocol is rejected with `-ENOTSUP`
(`osmosdr_serial.c:132-134`) instead of failing silently.

### 4.7 Console Commands

| Command | Location | Description |
|---|---|---|
| `sdr_capture` | `cmd_control.c:276-287` | Opens the native app using the exact same path as the menu button: `menus_module_set_menu_over_cmd("sdr")`, because the menu system owns the exit callback (launching the app outside the menu would leave it running indefinitely). |
| `sdtest` | `sd_card.c:110-117` | Mounts the microSD card and reports the result (section 14). |
| `rf_tx` | *removed* | Removed in `bce64f44`. |

`boot_sdr`, which handed boot over to `ota_1`, was replaced by `sdr_capture` in
`f4f780e1`.

### 4.8 The osmoSDR Protocol (`osmosdr_serial.c`)

Implementation of the HackRF-style binary protocol. **It is not used at runtime
by the firmware** (only exercised by the host test suite and kept as a
reference), but it documents the wire contract:

| Request | Value | Behavior |
|---|---|---|
| `ESDR_REQ_SET_MODE` | 0x02 | Accepts OFF/RX/TEST_IN/TEST_OUT. `ESDR_MODE_TX` returns `-ENOTSUP`. |
| `ESDR_REQ_SET_FREQ` | 0x03 | 8-byte LE in Hz; valid range 100-6000 MHz. |
| `ESDR_REQ_SET_SAMPLE_RATE` | 0x04 | 4-byte LE; 0 returns `-EINVAL`; rounds to `{250000, 125000, 62500, 31250, 15625}`. |
| `ESDR_REQ_SET_FORMAT` | 0x08 | Only 8-bit or 16-bit formats accepted (12-bit rejected). |
| `ESDR_REQ_MININO` | 0x7e | Triggers the `MININO` escape sequence (6 ASCII bytes) to return to Minino. |

The escape sequence is detected byte-by-byte with reset on partial match prior
to header parsing, and resynchronization on `ESDR_SER_MAGIC` (4 bytes) uses a
sliding `memmove`.

### 4.9 I/Q Packing (`vrt_packet/iq_pack.c`)

Supports three bit depths with their respective sample quanta:

| Bits | Layout | Quantum |
|---|---|---|
| 16 | 1 sample per word: `I` in 31:16, `Q` in 15:0 | 1 sample |
| 8 | 4 items per word: `I0<<24 \| Q0<<16 \| I1<<8 \| Q1` | 2 samples |
| 12 | 4 samples = 8 items = 96 bits = **3 words**, packed MSB to LSB | 4 samples |

Scaling uses `shift = floor(log2(full_scale)) - (bits-1)` with half-way
rounding and clamping to `[-lim, lim-1]`. `iq_pack_raw` uses
`full_scale = 1<<(bits-1)` -> `shift = 0` (lossless).

---

## 5. `radio_lease`: Owner of the 2.4 GHz Radio

### 5.1 The Two Problems

From `radio_lease.h:1-23`:

> **Shared ownership of the single 2.4 GHz radio.**
>
> Two independent locks, both reference counted:
> - **Promiscuous mode.** ESP-IDF keeps one promiscuous receive callback and
>   one on/off switch. Six subsystems need it. They used to call
>   `esp_wifi_set_promiscuous_rx_cb()`/`esp_wifi_set_promiscuous()` directly,
>   so the last one to start stole the callback from the others and a window
>   ending with `esp_wifi_set_promiscuous(false)` killed a capture that
>   somebody else was relying on. Now each consumer claims a slot and the
>   driver is only touched on the 0->1 and 1->0 transitions; frames are fanned
>   out to every holder.
> - **The WiFi driver and the BLE stack.** The surveillance detector rotates
>   between a WiFi window and a BLE window and used to
>   `esp_wifi_deinit()`/`esp_bluedroid_deinit()` in between. Those are global:
>   any app that merely happened to be running - the analyzer's sniffer, the
>   deauth detector, BLE spam, BLE HID, the trackers scanner - lost the radio
>   mid-operation. They now hold a lease, and the detector only takes the
>   driver down when it is the last holder.

### 5.2 The API

```c
typedef enum {
  RADIO_PROM_NONE = 0,
  RADIO_PROM_SURVEILLANCE,
  RADIO_PROM_SNIFFER,
  RADIO_PROM_DEAUTH_DETECTOR,
  RADIO_PROM_DRONEID_SCANNER,
  RADIO_PROM_SPECTRUM,
  RADIO_PROM_SDR_RX,
  RADIO_PROM_USER_COUNT
} radio_prom_user_t;
```

| Function | Return Value | Semantics |
|---|---|---|
| `radio_lease_prom_acquire(user, cb)` | `ESP_OK` / `ESP_ERR_INVALID_ARG` / driver error | Claims a slot. **The driver is only touched on the 0->1 transition.** |
| `radio_lease_prom_hold(user)` | same | Claims promiscuous mode **without** a packet callback (used by the SDR). |
| `radio_lease_prom_set_cb(user, cb)` | `ESP_OK` / `ESP_ERR_NOT_FOUND` | Updates the callback for an existing holder. |
| `radio_lease_prom_release(user)` | `ESP_OK` / `ESP_ERR_NOT_FOUND` | **The driver is only disabled on the 1->0 transition.** |
| `radio_lease_prom_count()` / `_held(user)` | | Queries state. |
| `radio_lease_prom_set_filter(filter)` | `ESP_OK` | Sets the filter without forcing promiscuous mode on/off. |
| `radio_lease_wifi_acquire(mode)` | `ESP_OK` / `ESP_ERR_NOT_SUPPORTED` / error | `STA` / `NULL` / `APSTA`. Initializes the driver only on 0->1. **Rejects if BLE has active holders.** |
| `radio_lease_wifi_release()` | - | Deinitializes only when the last claim is released. |
| `radio_lease_ble_acquire()` | `ESP_OK` / `ESP_ERR_NOT_SUPPORTED` / error | Brings up controller + Bluedroid on 0->1. **Rejects if WiFi has active holders**; only tears down the WiFi driver if nobody holds it (~65 KB freed). |
| `radio_lease_ble_release()` | - | Tears down Bluedroid + controller only on 1->0. Ignores orphan releases. |
| `radio_lease_ble_block_scan()` / `_unblock_scan()` / `_scan_allowed()` | | BLE scan gate (section 5.6). |

### 5.3 The Registry: Pure C, Host-Testable

`radio_lease_registry.h` has zero ESP-IDF dependencies:

```c
#define RADIO_LEASE_MAX_USERS 8

typedef struct {
  radio_lease_rx_fn fn[RADIO_LEASE_MAX_USERS];
  void* ctx[RADIO_LEASE_MAX_USERS];
  bool active[RADIO_LEASE_MAX_USERS];
  uint8_t count;
} radio_lease_registry_t;
```

**The consumer ID IS the slot index.** Acquiring twice from the same consumer
does not inflate the reference count: the Wi-Fi sniffer re-registers on every
start and must remain a single holder.

**Frame dispatch** (`radio_lease_registry.c:85-114`) snapshots subscribers
**under a lock** and invokes them **outside the lock**:

```c
  portENTER_CRITICAL(&s_mux);
  for (...) { if (reg->active[i] && reg->fn[i]) { fn[n] = reg->fn[i]; ctx[n] = reg->ctx[i]; n++; } }
  portEXIT_CRITICAL(&s_mux);

  for (uint8_t i = 0; i < n; i++) fn[i](ctx[i], buf, type);
```

The reason is documented in the source: the analyzer stops itself from inside
its own RX callback upon reaching its packet limit; holding the spinlock
across the callback would deadlock.

### 5.4 Atomic BLE Reference Counting

The reference count and the scan gate share **a single** spinlock
(`s_ble_mux`):

```c
static uint8_t s_ble_refs = 0;
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_ble_scan_block = 0;
```

`radio_lease.c:240-270` explains why the critical section is mandatory:

> The refcount is touched from the button state machine, the menus' on-exit
> callbacks, console commands and the BTC task, so every read-modify-write
> pair needs a critical section. Without it two racing stop/start paths could
> both see `before != 0` and skip the bring-up, or both drive the count to zero
> and tear the stack down under each other.

### 5.5 Integration with the Dispatchers

```
radio_lease_ble_acquire()  ->  gap_dispatcher_init()
radio_lease_ble_release()  ->  gap_dispatcher_reset()  +  gatt_dispatcher_reset()
```

`radio_lease.h:128-134` defines the separation of responsibilities: the stack
is brought up here, while **GAP callbacks are registered via
`gap_dispatcher`**, one per subscriber.

### 5.6 The Scan Gate

The BLE controller cannot scan and advertise (or set a random address) at the
same time: *"Advertising or scanning now, can't set randaddress"*. While an
advertiser holds the scan gate closed, **every** scanner in the codebase checks
`radio_lease_ble_scan_allowed()` before starting or re-arming a scan:

- `bt_gattc.c:382` (before starting a scan) and `:442` (when re-arming after
  `ESP_GAP_SEARCH_INQ_CMPL_EVT`)
- `ble_scann.c:71`
- `gattcmd_scan.c:97`, `gattcmd_enum.c:313`, `gattcmd_recon.c:359`,
  `gattcmd_write.c:223`

### 5.7 Consumer Mapping

| Consumer | Slot |
|---|---|
| `surv_radio.c:288/318` | `RADIO_PROM_SURVEILLANCE` |
| `cmd_sniffer.c:484/387` | `RADIO_PROM_SNIFFER` |
| `detector.c:118/152` | `RADIO_PROM_DEAUTH_DETECTOR` |
| `droneid_scanner.c:232/283` | `RADIO_PROM_DRONEID_SCANNER` |
| `rf_spectrum.c:361/392` | `RADIO_PROM_SPECTRUM` |
| `c6_sdr_rx.c:71/210` | **Wi-Fi lease + `RADIO_PROM_SDR_RX`** |

---

## 6. `event_bus`: Typed Event Bus

> **Removed in Phase 1.** The bus had no active subscribers with non-NULL
> callbacks (`wifi_service` macro registrations passed `NULL`). Apps now use
> direct callbacks and `esp_event`; this section is preserved for historical
> reference.

### 6.1 Overview

A lightweight event bus with a **per-topic subscriber table** and fan-out
delivery. Topics:

```c
typedef enum {
  EV_INPUT,
  EV_MENU,
  EV_WIFI,
  EV_GPS,
  EV_APP_START = 128,   /* reserved range for private app events */
} event_topic_t;

#define EVENT_MAX_SUBS 2
```

### 6.2 The API

| Function | Return Value | Semantics |
|---|---|---|
| `event_bus_init()` | `true` | Idempotent. Builds the topic-to-slot map **once**. |
| `event_bus_deinit()` | - | Clears the subscriber table and topic map. |
| `event_bus_subscribe(topic, cb, data)` | `bool` | Rejects `topic >= EV_APP_START`, invalid slots, duplicate `(cb, data)` pairs, and full tables. |
| `event_bus_unsubscribe(topic, cb)` | `bool` | Removes subscriber by swapping with the last entry. |
| `event_bus_dispatch(topic, data)` | - | Delivers the event to all subscribers of the topic. |

Convenience macros: `event_bus_post()`, `event_bus_post_input()`,
`event_bus_post_menu()`, `event_bus_post_wifi()`, `event_bus_post_gps()`.

### 6.3 Event Dispatch

```c
void event_bus_dispatch(event_topic_t topic, void* data) {
  (void) data;
  if (topic < 0 || topic >= EV_APP_START) return;

  event_subscriber_t subs[EVENT_MAX_SUBS];
  /* Copy under the lock, then call outside it: a subscriber that subscribes or
     dispatches from inside its own callback would otherwise deadlock on a
     non-recursive spinlock. */
  uint8_t n = _get_subs(topic, subs);
  for (uint8_t i = 0; i < n; i++) {
    if (subs[i].cb) subs[i].cb(topic, subs[i].data);
  }
}
```

Two design details worth noting:

1. **The `data` argument to `dispatch` is ignored.** Each subscriber receives
   **its own** `data` pointer registered during `subscribe`. `dispatch` acts
   purely as a signal.
2. **Subscribers are copied under the lock and invoked outside it**, allowing
   a subscriber to subscribe, unsubscribe, or dispatch from within its own
   callback without deadlocking on a non-recursive spinlock.

### 6.4 Topic-to-Slot Mapping

```c
static uint8_t _topic_slot(event_topic_t topic) {
  if (topic < 0 || topic >= EV_APP_START) return EVENT_MAX_SUBS;  /* sentinel */
  uint8_t idx = s_topic_index[topic];
  return (idx < MAX_TOPICS) ? idx : EVENT_MAX_SUBS;
}
```

The map is built **once** in `event_bus_init()`. The comment at
`event_bus.c:59-64` explains why it cannot be rebuilt on subsequent calls:
renumbering slots would leave existing subscribers stranded in a slot table
that nothing dispatches to anymore.

> **Bug discovered and fixed in section 22.1.** Originally, the invalid-slot
> sentinel was `EVENT_MAX_SUBS`. When `EVENT_MAX_SUBS` was reduced to `2`,
> slot index `2` became indistinguishable from the sentinel. Because
> `event_bus_init()` assigned `EV_INPUT->0, EV_MENU->1, EV_WIFI->2, EV_GPS->3`,
> `EV_WIFI` and `EV_GPS` were silently rejected until the sentinel was changed
> to `EVENT_SLOT_INVALID` (`0xff`). See section 22.1.

### 6.5 SRAM Footprint and Trimming

`sizeof(event_subscriber_t)` is 8 bytes on the 32-bit C6 ABI.

| Commit | `EVENT_MAX_SUBS` | `MAX_TOPICS` | `s_subs[]` | Total `.bss` | Delta |
|---|---|---|---|---|---|
| `92bc14f9` (initial) | 16 | 160 | 21,120 B | 21,248 B | - |
| `ba55fa02` | 8 | 160 | 10,880 B | 11,008 B | -10,240 B (10 KB) |
| `46d235a1` | 4 | 140 | 5,040 B | 5,168 B | -5,840 B |
| `cca3cc43` | **2** | 140 | 2,800 B | **2,928 B** | -2,240 B |

Cumulative savings: **17.9 KB of static SRAM freed**, helping the firmware
image fit below the capture bank.

### 6.6 Usage Across the Codebase

| Publisher | File |
|---|---|
| `EV_WIFI` | `wifi_service.c:42`, `wifi_app.c:51,74,87`, `OTA/modules/wifi_ap/wifi_ap.c:177`, `captive_module.c:545,711` |
| `EV_INPUT` / `EV_MENU` / `EV_GPS` | None |

| Subscriber | File |
|---|---|
| `wifi_service_on_state_change(cb, data)` | `wifi_ap_manager.c:86`, `wifi_scanner.c:28`, `captive_module.c:238,244`, `detector.c:95`, `droneid_scanner.c:212`, `modbus_dos.c:83` |

> **All `subscribe` call sites passed `cb = NULL`.** The bus accepted a NULL
> callback and stored it, while `dispatch` skipped it via `if (subs[i].cb)`.
> They served as placeholders rather than active callbacks.

> **Two misnamed commits.** `049500e5` ("subscribe sniffer screens to event
> bus") only added an unused `#define THREAD_SNIFFER_TOPIC (EV_APP_START + 2)`,
> which the bus would reject anyway for being `>= EV_APP_START`. Actual sniffer
> fan-out uses a private 4-slot table in `thread_sniffer.c:51`. `a7457dd0`
> ("align animation task with event bus") does not touch the event bus at all;
> it only modifies the animation task shutdown sequence.

### 6.7 Why `event_bus` Cannot Carry GAP Events

This lesson motivated the dedicated BLE dispatchers and is documented in
`test_event_bus.c:1-21`:

> event_bus was introduced as a replacement for direct GAP callback
> registration in the gattcmd console handlers. That substitution was wrong
> and silent ... the subscription was refused, the return value was discarded,
> and `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` was never handled, so no scan
> ever started.

GAP events must flow through `gap_dispatcher` (section 7).

---

## 7. `gap_dispatcher` and `gatt_dispatcher`: Bluedroid Fan-Out

### 7.1 The Problem

Bluedroid maintains **a single** GAP callback (`esp_ble_gap_register_callback`)
and **a single** GATTC callback (`esp_ble_gattc_register_callback`). When
multiple modules call those APIs directly, each call **overwrites** the
previous registration and earlier modules silently stop receiving events. This
was the root cause of recurring "BLE scanning/advertising stopped working"
bugs.

### 7.2 `gap_dispatcher`

```c
#define GAP_SUBSCRIBER_MAX 16

typedef struct {
  esp_gap_ble_cb_t cb;
  bool used;
} gap_subscriber_t;
```

| Function | Semantics |
|---|---|
| `gap_dispatcher_init()` | Registers the internal callback with Bluedroid **only when transitioning into `ENABLED`**. |
| `gap_dispatcher_register(cb)` | Returns `ESP_ERR_INVALID_ARG` (NULL), `ESP_ERR_NO_MEM` (full), or `ESP_OK`. Duplicates are ignored. |
| `gap_dispatcher_unregister(cb)` | Returns `ESP_ERR_NOT_FOUND` if not registered. |
| `gap_dispatcher_reset()` | **Stack-reset hook**: forgets the installed state so the next `register()` reinstalls the callback on a freshly enabled Bluedroid stack. |

Transition logic (`gap_dispatcher.c:63-73`):

```c
  esp_bluedroid_status_t status = esp_bluedroid_get_status();

  portENTER_CRITICAL(&s_dispatcher_mux);
  bool just_enabled = (status == ESP_BLUEDROID_STATUS_ENABLED) &&
                      (s_last_seen_status != ESP_BLUEDROID_STATUS_ENABLED);
  s_last_seen_status = status;
  portEXIT_CRITICAL(&s_dispatcher_mux);

  if (!just_enabled) return ESP_OK;
```

Prior to `3fbb6d9f`, it re-registered with Bluedroid on **every** `init()`
call, incurring an unnecessary round-trip to the BTC task per subscriber.

### 7.3 `gatt_dispatcher` (New in `3fbb6d9f`)

Applies the same fan-out pattern to the GATT client side:

```c
#define GATT_SUBSCRIBER_MAX 8

static gatt_subscriber_t s_subscribers[GATT_SUBSCRIBER_MAX];
static bool s_installed = false;
```

Dispatch forwards `gattc_if` to every subscriber so each handler can filter out
events intended for another module's interface:

```c
  for (uint8_t i = 0; i < count; i++) local_cbs[i](event, gattc_if, param);
```

### 7.4 Unique GATTC Application IDs

`esp_ble_gattc_app_register()` **rejects duplicate IDs**. `gattcmd_service.h:14-24`
documents why unique IDs are required:

> Unique Bluedroid GATTC app_ids. `esp_ble_gattc_app_register()` rejects
> duplicates, and two services sharing app_id 0 (this file used to define it
> and `bt_gattc.c` uses the same value for DEVICE_PROFILE) collided: the second
> registration failed outright or confused the single global gattc_if -> app
> demux. bt_gattc owns 0, the gatt console services take 2...5.

```c
#define GATTCMD_ENUM_APP_ID  2
#define GATTCMD_SCAN_APP_ID  3
#define GATTCMD_RECON_APP_ID 4
#define GATTCMD_WRITE_APP_ID 5
```

Prior to `574dfe5e`, all four `gattcmd` services used `app_id = 0`.

### 7.5 Registered Subscribers

| Module | GAP | GATT |
|---|---|---|
| `ble_hidd_main.c` | `:373` | - |
| `bt_gattc.c` | `:539` | `:550` |
| `bt_gatts.c` | `:547` | - |
| `bt_spam.c` | `:982` | - |
| `surv_radio.c` | `:394` | - |
| `gattcmd_scan.c` | `:213` | `:222` |
| `gattcmd_enum.c` | `:428` | `:438` |
| `gattcmd_recon.c` | `:568` | `:577` |
| `gattcmd_write.c` | `:350` | `:359` |

### 7.6 Memory Footprint

| Structure | Size |
|---|---|
| `gap_subscriber_t s_subscribers[16]` | 128 B |
| `gatt_subscriber_t s_subscribers[8]` | 64 B |
| `radio_lease_registry_t s_prom` | 76 B |

---

## 8. `wifi_service`: A Single `esp_wifi_init()`

### 8.1 The Problem

There were **13 separate calls** to `esp_wifi_init()` scattered across the
codebase. Each module initialized the driver independently with its own
configuration (`wifi_slim_init_config()` in the controller vs. default configs
elsewhere), causing conflicting reinitializations.

### 8.2 The API

```c
bool wifi_service_init(void);
bool wifi_service_deinit(void);

wifi_mode_t         wifi_service_mode_get(void);
wifi_interface_t    wifi_service_iface_get(void);
bool                wifi_service_is_up_get(void);
bool                wifi_service_is_sta_get(void);
bool                wifi_service_is_ap_get(void);
wifi_second_chan_t  wifi_service_primary_chan_get(void);
wifi_second_chan_t  wifi_service_secondary_chan_get(void);

#define wifi_service_on_state_change(cb, data) \
  event_bus_subscribe(EV_WIFI, (event_callback_t)(cb), (void*)(data))
```

### 8.3 Mode Selection

```c
#if CONFIG_STA_ENABLE
  s_state.mode = WIFI_MODE_STA;   s_state.is_sta = true;
#elif CONFIG_AP_ENABLE
  s_state.mode = WIFI_MODE_AP;    s_state.is_ap = true;
#else
  s_state.mode = WIFI_MODE_STA;   s_state.is_sta = true;   /* fallback */
#endif
```

> **Note.** Neither `CONFIG_STA_ENABLE` nor `CONFIG_AP_ENABLE` is defined
> anywhere in the project or ESP-IDF. Undefined macros evaluate to 0 in `#if`,
> so **the fallback branch is always taken**: the service always selects
> `WIFI_MODE_STA`. `NULL` and `APSTA` modes are configured through
> `radio_lease` (`wifi_driver_init_null()` / `wifi_driver_init_apsta()`).

### 8.4 Driver Buffers and NVS

```c
  cfg.static_rx_buf_num = 2;
  cfg.dynamic_rx_buf_num = 4;
  cfg.dynamic_tx_buf_num = 4;
  cfg.nvs_enable = false;
```

Buffer counts are reduced because the capture bank reserves 64-128 KB of SRAM;
every driver buffer comes out of the remaining heap.

Setting `nvs_enable = false` prevents a specific reinitialization failure
(documented in `wifi_service.c:75-80`):

> The driver keeps a cookie with the calibration data in the NVS partition, and
> it opens it with the partition already open the second time round (the
> Bluetooth brings the driver down and up again): the second
> `esp_wifi_init()` then fails with `init nvs: failed, ret=101` and the Wi-Fi
> apps do not start. Without the cookie the driver recalibrates on every boot,
> which is a few hundred milliseconds.

### 8.5 Migration Pattern

Six commits titled *"defer wifi init to wifi_service"* replaced direct
`esp_wifi_init()` calls with a service check:

```c
  /* WiFi stack already initialized by wifi_service_init() in app_main(). */
  if (!wifi_service_is_up_get()) {
    ESP_LOGI(TAG_WIFI_DRIVER, "WiFi stack not initialized by service; initializing...");
    wifi_service_init();
  } else {
    ESP_LOGI(TAG_WIFI_DRIVER, "WiFi stack already initialized by service; skipping esp_wifi_init");
  }
```

Migrated modules: `wifi_controller.c`, `wifi_app.c`, `wifi_ap_manager.c`,
`wifi_scanner.c`, `OTA/modules/wifi_ap/wifi_ap.c`, `captive_module.c`,
`modbus_dos.c`, `deauth/detector.c`, `droneid_scanner.c`.

> Two of the six commits are misnamed: `cdd1b586` and `52138a31` do not touch
> `wifi_service` at all.

### 8.6 Boot Initialization

`firmware/main/main.c:55` called `event_bus_init()` as the **first** step in
`app_main()`:

```c
  /* Must run before anything dispatches: the topic -> slot map is what keeps
     subscribers of different topics in separate tables. */
  event_bus_init();
```

---

## 9. Module Lifecycle Hardening

### 9.1 The 12 Hardening Patterns

"Hardening" in this branch consists of twelve mechanical patterns applied
consistently across modules:

| # | Pattern | Canonical Example |
|---|---|---|
| 1 | Check `xTaskCreate` return value and guard the handle before `vTaskSuspend`/`vTaskResume` | `warbee_module.c:370-384` |
| 2 | Prevent self-delete race: `volatile bool running` + publish/clear handle under critical section | `bt_spam.c:1053-1069` |
| 3 | Exit symmetry: everything allocated in `begin()` is released in `exit()` | `warthread_module.c:459-475` |
| 4 | NULL guards on context/handle pointers | `modbus_tcp_scenes.c:100-110` |
| 5 | Bounds checking and index clamping | `modbus_tcp_prefs.c:26-45` |
| 6 | Division clamping against zero or underflow | `memory_monitor.c:29-33` |
| 7 | Stack sizing and VLA elimination | `ssid_spam.c:187-191` |
| 8 | State privatization: file-scope `static` + accessor functions | `radio_selector.c:6-9` |
| 9 | Moving data tables out of headers | `wifi_bitmaps.c/.h` |
| 10 | Global resource arbitration | `radio_lease`, `event_bus`, `wifi_service` |
| 11 | Moving blocking work off the `esp_timer` task | `keyboard_module.c:37-124` |
| 12 | Host unit tests for each invariant | 16 new test suites |

### 9.2 `vTaskSuspend(NULL)`: Freezing the UI

This was the most widespread bug. Passing `NULL` to `vTaskSuspend(NULL)`
**suspends the calling task**, not a task with a `NULL` handle. And the calling
task was typically the `esp_timer` task that dispatches button callbacks:

```c
/* before */
xTaskCreate(wardriving_screens_zigbee_animation_task,
            "scanning_wifi_animation_task", 4096, NULL, 5,
            &scanning_zigbee_animation_task_handle);
vTaskSuspend(scanning_zigbee_animation_task_handle);   /* NULL => suspends caller! */
```

```c
/* after - warbee_module.c:374-384 */
/* The handle must be checked before suspending it: vTaskSuspend(NULL)
   suspends the *calling* task, which here is the esp_timer task that runs
   the button callbacks - the whole UI would freeze with no way out. */
if (xTaskCreate(..., &scanning_zigbee_animation_task_handle) == pdPASS) {
  vTaskSuspend(scanning_zigbee_animation_task_handle);
} else {
  ESP_LOGE("Warbee", "could not create the animation task");
  scanning_zigbee_animation_task_handle = NULL;
}
```

Fixed in: `war_bee`, `war_thread`, `gps/wardriving`.

### 9.3 `warthread`: Radio Going Permanently Deaf

This was the deepest fix in the hardening campaign. The OpenThread packet
callback was performing SD card file writes and full OLED screen redraws
**while holding the OpenThread lock**:

```c
/* before */
static void warthread_packet_handler(const otRadioFrame* aFrame, ...) {
  ...
  sd_card_write_file(csv_file_name, csv_file_buffer);   /* fopen/fwrite/fclose */
  oled_screen_display_show();                          /* full I2C redraw */
}
```

`openthread_enable_promiscuous_mode()` delivers frames from
`otPlatRadioReceiveDone()` **on the OpenThread mainloop task with
`esp_openthread_lock` held**. Writing to the SD card (`fopen`/`fwrite`/`fclose`
per frame) and redrawing the OLED stalled the thread servicing all OpenThread
calls, overflowed the 20-entry driver RX queue, and left the `esp_ieee802154`
receive buffers wedged with `process == true`, rendering **the radio
permanently deaf after 20 frames**.

```c
/* after - warthread_module.c:194-238 */
/*
 * Runs outside the OpenThread lock.
 * ...
 * So the callback only copies the PSDU and a worker does the rest.
 */
static void warthread_packet_handler(const otRadioFrame* aFrame, bool aIsTx, void* aContext) {
  if (aFrame == NULL || aFrame->mLength < 16) return;
  if (warthread_frame_queue == NULL || gps_ctx == NULL || gps_ctx->sats_in_use == 0) return;
  warthread_frame_t item;
  item.len = (aFrame->mLength > WARTHREAD_PSDU_MAX) ? WARTHREAD_PSDU_MAX : aFrame->mLength;
  memcpy(item.psdu, aFrame->mPsdu, item.len);
  /* Zero timeout: never stall the OT task, drop the frame if the worker is behind. */
  xQueueSendToBack(warthread_frame_queue, &item, 0);
}
```

This introduced a frame queue + worker task, PSDU bounds checking
(`WARTHREAD_BASE_FRAME_LEN 16`), and full teardown of both the queue and worker
task in `warthread_module_exit()`.

### 9.4 `screen_saver`: The Black Rectangle Bug

`screen_saver_stop()` previously only cleared a boolean flag. However, the
animation task finished its current iteration (`display_bitmap()` +
`display_show()`) before rechecking the flag, overwriting the newly rendered
menu with a black frame:

```c
/* after - screen_saver.c:150-181 */
/* Clear the flag first so the task leaves its loop, then wait for it to be
   gone via the exit semaphore. Clearing the flag alone is not enough: ...
   That is exactly what happened once the button callbacks moved off the
   esp_timer task (priority 22) into a dedicated one (priority 4): the
   animation at priority 5 preempted the menu render. */
screen_saver_running = false;
if (screen_saver_task_handle != NULL) {
  if (screen_saver_exit_sem != NULL) {
    if (xSemaphoreTake(screen_saver_exit_sem, pdMS_TO_TICKS(1000)) != pdTRUE) { ... }
  } else { /* poll fallback */ }
  screen_saver_task_handle = NULL;
}
```

Associated changes: added binary semaphore `screen_saver_exit_sem`, published
`screen_saver_task_handle`, lowered priority from 5 to 4, and persisted
`IDLE_TIMEOUT_S` in NVS.

### 9.5 `trackers_scanner`: Self-Delete Race Condition

```c
/* after - trackers_scanner.c:98-136 */
/* Detach from stop() before vanishing: stop() copies this handle and
 * deletes the task; a self-delete while stop() still holds the copied
 * handle would turn that delete into a delete of an already-freed TCB,
 * so the handle is cleared (and stop() re-validated against it) under the
 * stop guard before vTaskDelete(NULL). */
portENTER_CRITICAL(&trackers_stop_mux);
trackers_scan_timer_task = NULL;
portEXIT_CRITICAL(&trackers_stop_mux);
vTaskDelete(NULL);
```

And `stop()` revalidates under the same spinlock:

```c
portENTER_CRITICAL(&trackers_stop_mux);
TaskHandle_t task_to_delete = trackers_scan_timer_task;
if (task_to_delete != NULL && task_to_delete != xTaskGetCurrentTaskHandle()) {
  trackers_scan_timer_task = NULL;
} else { task_to_delete = NULL; }
portEXIT_CRITICAL(&trackers_stop_mux);
if (task_to_delete != NULL) vTaskDelete(task_to_delete);
```

A spinlock is used instead of a mutex/semaphore to avoid priority inversion or
blocking during task self-deletion.

### 9.6 `bt_spam`: Preventing Accidental `vTaskDelete(NULL)` by the Caller

```c
/* after - bt_spam.c:1052-1071 */
if (adv_task_running) {
  uint32_t retries = 200; /* 200 x 10ms = 2s max wait */
  while (adv_task_running && retries-- > 0) vTaskDelay(pdMS_TO_TICKS(10));
  if (adv_task_running) {
    ESP_LOGW(TAG, "Adv task didn't exit cleanly, forcing delete");
    /* Snapshot the handle and only delete while it is still published:
     * the task NULLs its own handle right before self-deleting, and
     * vTaskDelete(NULL) would delete THIS task (the caller, usually the
     * menus thread) instead. */
    TaskHandle_t task_to_delete = adv_task_handle;
    if (task_to_delete != NULL && task_to_delete != xTaskGetCurrentTaskHandle()) {
      vTaskDelete(task_to_delete);
    }
    adv_task_handle = NULL;
    adv_task_running = false;
  }
}
```

`adv_task_running` is marked `volatile` and kept separate from the handle
because `xTaskCreate()` takes a plain `TaskHandle_t*`, and `volatile` prevents
the compiler from hoisting the load out of the polling loop.

### 9.7 `ssid_spam`: Malformed Beacon Frame Construction

Previously, `beacon_raw` was a flat 57-byte array containing **both the header
and the trailing IEs** (Supported-Rates / DS-Param / TIM). The SSID string was
appended at offset 57, while `beacon[37]` (the SSID length byte) pointed into
the middle of the TIM IE: **every transmitted beacon advertised a bogus SSID**
and the intended SSID strings were appended as trailing garbage.

```c
/* after - ssid_spam.c:62-80, :205-226 */
#define BEACON_SSID_OFFSET 38
_Static_assert(BEACON_SSID_OFFSET == sizeof(beacon_raw), "...");
#define BEACON_MAX_SSID_LEN 32     /* 802.11 limit */

memcpy(beacon, beacon_raw, sizeof(beacon_raw));
memcpy(beacon + BEACON_SSID_OFFSET, ssids_attack[line], ssid_len);
memcpy(beacon + BEACON_SSID_OFFSET + ssid_len, beacon_tail, sizeof(beacon_tail));
beacon[BEACON_SSID_LEN_OFFSET] = (uint8_t) ssid_len;

size_t frame_len = BEACON_SSID_OFFSET + ssid_len + sizeof(beacon_tail);
esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, beacon, frame_len, false);
if (err != ESP_OK) {
  /* The return value used to be discarded, so a spam that transmitted
     nothing looked exactly like one that worked. */
  ESP_LOGW(TAG, "beacon TX failed: %s", esp_err_to_name(err));
}
```

Additional fixes: removed the VLA (`uint16_t seqnum[MAX_STRINGS]` instead of
`seqnum[total_lines]`), added `spam_running` + `ssid_spam_stop_task()` for safe
re-entry, and updated `flash_storage_get_list()` to take an explicit capacity.

### 9.8 `modbus_tcp`: Invalid MBAP Length Field

The MBAP header *Length* field was previously calculated as `6 + byte_count`;
the Modbus TCP specification requires `7 + byte_count` (UnitId(1) +
PDU(6+byte_count)). The old value was 1 byte short, causing **compliant PLCs to
reject the frame**.

```c
/* after - modbus_engine_cmd.c:68-84, :118 */
/* 13 header bytes + 2 per register / 1 per 8 coils. */
size_t payload_bytes = (function_code == 0x0F) ? ((size_t) num_values + 7) / 8
                                                : (size_t) num_values * 2;
if (13 + payload_bytes > MODBUS_TCP_MAX_REQUEST_LEN) {
  ESP_LOGE(TAG, "too many values: %d would need %u bytes, the buffer holds %u", ...);
  return 1;
}
uint8_t request[MODBUS_TCP_MAX_REQUEST_LEN] = {0};
...
data_len = 7 + byte_count; /* Length = 7 + byte_count */
```

Additionally, `MODBUS_TCP_MAX_REQUEST_LEN 260` was defined as a named constant,
`request_len` loaded from NVS is now **clamped** before being used as a `memcpy`
length, and `modbus_engine_get_ctx()` returns early on `NULL` instead of
dereferencing it.

### 9.9 `catdos`: Memory Leak and Out-of-Bounds Write

```c
/* before */
for (int i = 0; i < aps_count; i++) { free(wifi_list[i]); }   /* aps_count comes from NVS */
...
aps_count = preferences_get_int("count_ap", 0);
for (int i = 0; i < aps_count; i++) {
  char wifi_ap[100]; sprintf(wifi_ap, "wifi%d", i);
  preferences_get_string(wifi_ap, wifi_ssid, 100);
  wifi_list[i] = strdup(wifi_ssid);      /* leaks on re-entry; OOB if aps_count > 20 */
}
```

```c
/* after - catdos_module.c:415-448 */
if (aps_count > 20) { aps_count = 20; }
for (int i = 0; i < 20; i++) {
  if (wifi_list[i] != NULL) { free(wifi_list[i]); wifi_list[i] = NULL; }
}
int loaded = 0;
for (int i = 0; i < aps_count; i++) {
  char wifi_ap[32]; char wifi_ssid[100];
  snprintf(wifi_ap, sizeof(wifi_ap), "wifi%d", i);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, sizeof(wifi_ssid));
  if (err != ESP_OK) continue;
  wifi_list[loaded] = strdup(wifi_ssid);
  if (wifi_list[loaded] != NULL) loaded++;
}
aps_count = loaded;
```

Additional fixes: increased request buffer from 128 to 256 bytes with
`snprintf` return checks, fixed `arg_end(2)` -> `arg_end(3)` (argtable
off-by-one), and enforced a 32-byte limit on host/port/endpoint strings.

### 9.10 `uart_bridge`: Non-Blocking Log Sink and Return Value Fix

```c
/* before */
int custom_log(const char* format, va_list args) {
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  uart_bridge_write(buffer, strlen(buffer));
  return 0;                       /* bug: vprintf callback must return formatted length */
}
```

```c
/* after - uart_bridge.c:74-124 */
int custom_log(const char* format, va_list args) {
  char buffer[256];
  va_list args_copy; va_copy(args_copy, args);
  int needed = vsnprintf(buffer, sizeof(buffer), format, args_copy);
  va_end(args_copy);
  if (needed < 0) return 0;
  size_t len = strlen(buffer);
  if (len > 0) {
    if (uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(5)) != ESP_OK) {
      return needed;              /* drop the line rather than stalling every logger */
    }
    uart_write_bytes(UART_NUM_0, buffer, len);
  }
  return needed;
}
```

The 5 ms timeout on `uart_wait_tx_done` is critical because `uart_write_bytes()`
calls `xRingbufferSend(..., portMAX_DELAY)`, and this log sink is invoked by
**every** `ESP_LOGx` call in the firmware, including those on the `esp_timer`
task.

Additional fixes: replaced VLA `char buffer[buffer_size]` with
`malloc(buffer_size + 1)` capped at `UART_BRIDGE_MAX_BUFFER_SIZE 4096`,
replaced `assert()` with explicit error checks, and fixed `println` parsing
against the wrong argument table.

### 9.11 `detector`: Shutdown Ordering

```c
/* before - detector.c:126-147 */
esp_wifi_set_promiscuous(true);
esp_wifi_set_promiscuous_rx_cb(packet_handler);
...
/* stop */
esp_wifi_stop();
esp_wifi_set_promiscuous(false);   /* kills capture for OTHER subsystems */
```

```c
/* after - detector.c:114-121, :146-153 */
if (radio_lease_prom_acquire(RADIO_PROM_DEAUTH_DETECTOR, packet_handler) != ESP_OK) {
  ESP_LOGE(TAG, "could not claim promiscuous mode");
  return;
}
...
/* Order matters: esp_wifi_stop() leaves the driver initialised but stopped,
   and the analyzer's sniffer then calls esp_wifi_set_promiscuous() /
   esp_wifi_set_channel() against a stopped driver - both fail silently and
   the packet counter stays at zero. */
radio_lease_prom_release(RADIO_PROM_DEAUTH_DETECTOR);
esp_wifi_stop();
```

### 9.12 `memory_monitor`: Clamping Division Underflow

```c
/* before */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0) return 100.0f;
  return ((float)(total_free - largest_block) / (float)total_free) * 100.0f;
}
```

```c
/* after - memory_monitor.c:29-33 */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0 || largest_block >= total_free) {
    return (total_free == 0) ? 100.0f : 0.0f;
  }
  return ((float)(total_free - largest_block) / (float)total_free) * 100.0f;
}
```

Additionally, `heap_monitor_start()` and `stop()` now acquire a mutex around
the entire state transition, releasing it on **all** early-return paths.

### 9.13 `resistor_detector`: Extracting Pure Math Outside `#ifdef ESP_PLATFORM`

The calculation logic was extracted outside `#ifdef ESP_PLATFORM` into
host-testable functions:

- `resistor_calculate_pullup_adc()`
- `resistor_calculate_pulldown_adc()`
- `resistor_calculate_voltage_divider()`
- `resistor_detector_evaluate_samples()`

With an explicit status enum:

```c
typedef enum {
  RESISTOR_CALC_OK,
  RESISTOR_CALC_ERROR_INVALID_ARG,
  RESISTOR_CALC_ERROR_DIV_ZERO,
  RESISTOR_CALC_ERROR_OUT_OF_RANGE,
  RESISTOR_CALC_OPEN_CIRCUIT,
  RESISTOR_CALC_SHORT_CIRCUIT
} resistor_calc_status_t;
```

`adc_raw == 0` -> open circuit; `adc_raw == adc_max` -> short circuit;
`adc_max == 0` -> `DIV_ZERO`. Covered by 12 host unit tests.

### 9.14 Catalog of Hardened Modules

| Module | Commit | Fixes Applied |
|---|---|---|
| `war_bee` | `857a17a9` | Guarded `vTaskSuspend(NULL)` and unchecked `xTaskCreate` |
| `war_thread` | `6758054b` | Moved heavy I/O out of the OpenThread lock (section 9.3) |
| `settings` + `screen_saver` | `7d4b08f2` | Synchronous stop contract via exit semaphore (section 9.4) |
| `web_file_browser` | `a3bf17f2` | Moved header-defined variable to `extern` + `.c` definition |
| `cat_dos` | `e0b79f30` | Fixed `strdup` leak and OOB write from NVS `aps_count` (section 9.9) |
| `file_manager` + `files_ops` | `b0163862`, `2c177f0d` | Overflow-safe sizes, path traversal checks, 23 host tests |
| `modbus_tcp` | `e73754f7` | Fixed MBAP length and clamped `request_len` (section 9.8) |
| `ssid_spam` | `f5c2747f` | Fixed malformed beacon frame construction (section 9.7) |
| `uart_bridge` | `1a763db1`, `c9579d2b` | Non-blocking log sink and accurate return length (section 9.10) |
| `deauth detector` | `1f86aba6` | Promiscuous lease claim and shutdown ordering (section 9.11) |
| `memory_monitor` | `f3c3c248` | Division clamping and state mutex (section 9.12) |
| `trackers` | `6a078453`, `6c958e7e` | Fixed self-delete race (section 9.5) and BLE->SD init order |
| `wifi_analyzer` | `931260ad` | SD mount failure treated as fatal; unconditional `close_file` |
| `bt_spam` | `280368ca`, `913e2d81` | Task stack sizing, task-delete race, error-path cleanup |
| `resistor_detector` | `c69437cc` | Extracted and guarded pure math functions (section 9.13) |
| `gps` / `wardriving` | `00a2b83d` | `CSV_LINE_SIZE` 150->256, removed VLA, `strcat`->`strncat`, stack buffers |
| `open_thread` | `339f0dbd` | Made `channel` `static`; subscribed to event bus |
| `oled_screen` | `69427bb5` | Privatized state to `static`, added `oled_screen_ink()` |
| `radar_selector` | `e2165751` | Privatized state to `static` |
| `flock / surveillance` | `7ce2761d`, `ddf071ca` | Added missing exit callback; released BLE stack on exit |
| `drone_id` | `80d47bbf` | Privatized shared driver symbols to `static` |

---

## 10. State Privatization

### 10.1 The Pattern

*File-scope `static` variables + accessor functions.* Each variable keeps its
name and file, changing linkage from external to internal and exposing a clean
getter/setter pair where needed.

```c
/* radio_selector.c */
-uint8_t radio_selected_option;
+static uint8_t radio_selected_option;
 uint8_t radio_selector_get_selected_option() { return radio_selected_option; }
```

### 10.2 Privatized Symbols

| Module | Symbols | Commit |
|---|---|---|
| `sd_card.c` | `_format_if_mount_failed`, `_sd_card_info` | `080d8b29` |
| `open_thread.c` | `mAddr`, `dataset` | `010e3446` |
| `radio_selector.c` | `radio_selected_option` | `e2165751` |
| `oled_screen.c` | `dev`, `oled_mutex` | `69427bb5` |
| `gps_module.c` | `nmea_hdl`, `gps_event_callback`, `TIME_ZONES[]` | `00a2b83d` |
| `gps_screens.c` | `gps_help_menu` | `00a2b83d` |
| `gps/wardriving/*` | 12 symbols | `00a2b83d` |
| `open_thread_module.c` | `channel` | `339f0dbd` |
| `warbee_module.c` | `csv_file_name`, `csv_file_buffer` | `857a17a9` |
| `warthread_module.c` | `csv_file_name`, `csv_file_buffer` | `6758054b` |
| `settings_module.c` | `SD_inf` | `7d4b08f2` |
| `sd_card_settings_module.c` | `state` | `7d4b08f2` |
| `web_file_browser.c` | `mount_path`, `show_hidden_files` | `a3bf17f2` |
| `wifi_screens_module.c` | `wifi_sniffer_animation_task_handle`, `pkts_offset` | `fd139212` |
| `uart_bridge_app.c` | 4 option bytes | `1a763db1` |
| `drone_id_anim.c` | `drone_anim_order`, `drone_anim_durations_ms`, `drone_animation` | `d16af92c` |
| `OTA.c` | `ota_show_event_cb` | `0beea4cc` |
| `OTA/http_server.c` | `fw_update_reset` | `0beea4cc` |
| `uart_bridge.c` | `uart_bridge_config` | `c9579d2b` |

### 10.3 Moving Data Out of Headers (Eliminating `-zmuldefs`)

**The problem.** Bitmap arrays and pointer tables were *defined* (not merely
declared) inside header files. Every translation unit including those headers
emitted **its own duplicate copy**; the linker only accepted this because
`components/wifi_attacks/CMakeLists.txt` passed `-Wl,-zmuldefs` to **the entire
firmware build**. That flag also masked ~70 other duplicate symbol definitions.

**The solution.** `extern` declarations in the `.h` file paired with a single
definition in a companion `.c` file:

```c
/* Definitions split out of main/apps/wifi_analyzer/wifi_bitmaps.h by hand: this data was defined in
   the header, so every translation unit that included it emitted
   its own copy and the linker only accepted it thanks to
   -Wl,-zmuldefs. Keep the data here and the declarations in the
   header. */
```

| Header | New `.c` File | Data Moved |
|---|---|---|
| `wifi_bitmaps.h` (364 -> 15 lines) | `wifi_bitmaps.c` (+349) | ~30 bitmap arrays + `wifi_bitmap_allArray[4]` |
| `deauth_screens.h` | `deauth_data.c` (73) | 4 x 32x32 frames, `deauth_menu[]`, `deauth_attacks[]` |
| `drone_id_bmps.h` | `drone_id_bmps.c` (24) | `drone_id_bmp_1..3_16x8[]` |
| `zigbee_bitmaps.h` | `zigbee_bitmaps.c` (65) | 4 x `zigbee_bitmap_loading_1..4[]` |
| `bitmaps_general.h` | `bitmaps_general.c` (244) | `minino_face[]`, logos, `general_bt_wifi_loading[4]` |
| `menus_include/menus.h` | `menus_data.c` (694) | **The entire** `menu_t menus[]` catalog (2.3 KB) |

In `7a9aaeff`, `-zmuldefs` was removed from the build and replaced with the
targeted linker wrap it was originally intended for:

```cmake
target_link_libraries(${COMPONENT_LIB} "-Wl,--wrap=ieee80211_raw_frame_sanity_check")
```

---

## 11. UI: Menu Catalog, Views, Icons, and OLED

### 11.1 Moving the Menu Catalog Out of the Header

`menus_include/menus.h` previously *defined* `menu_t menus[]` (2.3 KB). Because
`menus_module.h` included `menus.h`, every one of the ~64 translation units
including it received its own copy of the table.

Now (`f708fd5f`), the catalog lives in `menus_data.c` and is accessed via
functions:

```c
menu_t* menus_get(void);
uint8_t menus_count(void);
int menus_find_index(menu_idx_t id);
menu_t* menus_find(menu_idx_t id);
```

Guarded by two `_Static_assert` checks:

```c
_Static_assert(sizeof(menus) / sizeof(menus[0]) <= MENU_COUNT,
               "menu catalogue has more entries than MENU_COUNT");
_Static_assert(sizeof(menus) / sizeof(menus[0]) < 256,
               "menus_count() truncates at 256 entries");
```

> `menus_count()` returns `uint8_t`, so at 256 or more entries the count would
> wrap around (e.g., 260 -> 4), rendering most of the menu tree unreachable.

### 11.2 Menu Entry Structure

```c
typedef struct {
  menu_idx_t   menu_idx;
  menu_idx_t   parent_idx;
  const char*  display_name;
  const char*  entry_cmd;
  uint8_t      last_selected_submenu;
  menu_cb_t    on_enter_cb;      /* previously void* */
  menu_cb_t    on_exit_cb;
  bool         is_visible;
} menu_t;
```

`menu_cb_t` was changed from `void*` to `void (*)(void)` so the compiler
enforces callback signatures.

Entries inside the catalog array are conditionally compiled:

```c
#if defined(CONFIG_SDR_ENABLE)
{.display_name = "SDR", .menu_idx = MENU_ESP_SDR, .parent_idx = MENU_APPLICATIONS,
 .entry_cmd = "sdr", .on_enter_cb = esp_sdr_module_begin,
 .on_exit_cb = esp_sdr_module_stop, .is_visible = true},
#endif
{.display_name = "Spectrum", .menu_idx = MENU_SPECTRUM, .parent_idx = MENU_APPLICATIONS,
 .entry_cmd = "spectrum", .on_enter_cb = rf_spectrum_app_begin,
 .on_exit_cb = rf_spectrum_app_stop, .is_visible = true}
```

And `menus_data.c:53-58` explains why `#include "sdkconfig.h"` is explicit:

> Most of the catalogue below is gated on CONFIG_* symbols. In the IDF build
> they arrive through the toolchain, but relying on `<stdio.h>` to drag
> `sdkconfig.h` in is fragile: dropping that include would silently compile
> out ~60 entries with no diagnostic.

### 11.3 Exact Matching on `entry_cmd`

```c
/* menus_module.c:54-68 */
/* Exact match: the old code compared only strlen(entry_cmd) bytes, so
   "deauth_scan" matched the "deauth" entry first and launched the wrong
   app. */
if (!strcmp(catalogue[i].entry_cmd, entry_cmd)) return i;
```

### 11.4 `menu_at_checked()`

Previously, a failed `get_menu_idx()` lookup **fell back to index 0 (root)**.
Hiding a non-existent entry therefore hid the root menu and left `current_menu`
at `-1`, trapping navigation. There are now two helpers: `menu_at()` (which
falls back to root for read paths) and `menu_at_checked()` (which returns
`NULL` and logs an error), used by all mutating callers (hide/expose).

### 11.5 Menu View Styles

Four selectable styles are registered in `menu_views/menu_views.c:120-133`:

```c
static const menu_view_t s_views[] = {
    {"Classic",    render_classic},
    {"Icon List",  render_icon_list},
    {"Grid",       render_grid},
    {"Hero Dock",  render_hero},
};
```

| Index | Name | Layout Geometry |
|---|---|---|
| 0 | Classic | Delegates to the original text list renderer |
| 1 | Icon List | 4 rows x 13 px, icon at x=3, label at x=18, selection frame, scrollbar on the right |
| 2 | Grid | 4x2 icon grid, 32x26 cells, 8 per page, double selection border |
| 3 | Hero Dock | Large icon at (6,8) + title scaled x2 if it fits within 104 px, `"%d of %d"` counter, 8-icon dock at y=40 |

The active style is persisted in NVS under key `"menu_view"` (uint8, 0 =
Classic). The single hook point is `display_menus()` in
`menus_module.c:88-100`:

```c
  if (menus_ctx != NULL && menus_ctx->submenus_count == 0) {
    update_menus();
    if (menus_ctx->selected_submenu >= menus_ctx->submenus_count) {
      menus_ctx->selected_submenu = menus_ctx->submenus_count > 0 ? menus_ctx->submenus_count - 1 : 0;
    }
  }
  menu_views_render(menus_ctx);
  oled_screen_display_show();
```

Views are **purely rendering layers**: navigation, tree structure, and
entry/exit callbacks remain unchanged.

The user setting lives in `settings/menu_style/menu_style.c` and reuses
`general_radio_selection`.

### 11.6 `menu_gfx.c` and `menu_icons.c`

**`menu_gfx.c`** (73 lines) provides pixel-coordinate drawing utilities:

| Function | Description |
|---|---|
| `menu_gfx_text(x, y, s, scale, lit)` | Renders text at **pixel** (not page) coordinates with integer scaling using `font8x8_basic_tr[]` |
| `menu_gfx_text_width(s, scale)` | `strlen(s) * 8 * scale` |
| `menu_gfx_text_clip(x, y, s, maxpx, lit)` | Truncates string to `maxpx/8` characters without wrapping |
| `menu_gfx_frame(x, y, w, h)` | Draws a cornerless 4-line frame |
| `menu_gfx_scrollbar(n, rows, top)` | Draws a 3x52 px scrollbar at x=124, y=11 |

**`menu_icons.c`** (386 lines) defines 5 drawing primitives (`p`, `ln`, `rc`,
`circ`, `arc`) and **43 procedural glyphs**: 11 category icons (`ic_wifi`,
`ic_bt`, `ic_zig`, `ic_thread`, `ic_gps`, `ic_chip`, `ic_folder`, `ic_apps`,
`ic_gear`, `ic_info`, `ic_dot`) and 32 action icons (`ic_scan`, `ic_waves`,
`ic_bolt`, `ic_play`, `ic_doc`, `ic_download`, `ic_card`, `ic_clock`,
`ic_speed`, `ic_route`, `ic_sat`, `ic_bulb`, `ic_toggle`, `ic_moon`,
`ic_stealth`, `ic_zzz`, `ic_drone`, `ic_cam`, `ic_key`, `ic_tag`, `ic_lock`,
`ic_term`, `ic_monitor`, `ic_list`, `ic_disk`, `ic_apple`, `ic_android`,
`ic_windows`, `ic_samsung`, `ic_all`, `ic_check`, `ic_back`).

The lookup table maps **70 entries** by menu `display_name`:

```c
menu_icon_fn menu_icon_for(const char* n) {
  for (unsigned i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
    if (!strcmp(MAP[i].name, n)) return MAP[i].fn;
  return ic_dot;
}
```

> Note: `ic_moon` and `ic_back` are defined but not referenced in `MAP`.

### 11.7 OLED: SH1106 Support and I2C Address Probing

SH1106 support (`05c33cbb`) is implemented in the **OLED driver** rather than
`oled_screen`. The controller type is selected via Kconfig:

```
choice DRIVER
    prompt "Driver"
    default SH1106 if BOARD_MININO
    default SSD1306 if !BOARD_MININO
```

Dynamic I2C address probing replaces a hardcoded address:

```c
// oled_driver_i2c.c:56-66
if (i2c_probe_address(0x3C)) {
  dev->_address = 0x3C;
  ESP_LOGI(TAG, "OLED detected at I2C address 0x3C");
} else if (i2c_probe_address(0x3D)) {
  dev->_address = 0x3D;
  ESP_LOGI(TAG, "OLED detected at alternative I2C address 0x3D");
} else {
  dev->_address = I2CAddress;
  ESP_LOGW(TAG, "OLED not responding at 0x3C or 0x3D! Check VCC/GND wiring and pullups.");
}
```

Probing sends START -> address byte -> STOP with a 50 ms timeout. The SPI path
does not probe.

Differences between the two controllers:

| Aspect | SSD1306 | SH1106 |
|---|---|---|
| Charge pump | `0x8D` + `0x14` | `0xAD` + `0x8B` (internal DC-DC **on**) + `0x32` (8.0 V) |
| Precharge | Not sent | `0xD9 0x1F` |
| Contrast | `0x81 0xFF` | `0x81 0x80` |
| Memory addressing mode | `0x20 0x02` + column start | Not sent (unsupported on SH1106) |
| Column offset | None | **+2** (132-column internal RAM with 128-px panel centered) |
| COM scan direction | Always `0xC8` | Conditional on `dev->_flip` |

The +2 column offset is applied when pushing pixels (`oled_driver_i2c.c:193-197`):

```c
  #ifdef CONFIG_SSD1306
    int _seg = seg;
  #elif CONFIG_SH1106
    int _seg = seg + 2;
  #endif
```

### 11.8 `oled_screen_ink()`

```c
/*
 * How much ink is in the frame buffer that was last pushed to the panel,
 * counted as non-zero bytes.
 *
 * Lets a caller (a console command, a test, the field) tell "the panel is dead"
 * apart from "the panel has nothing drawn on it", which look identical from
 * the outside. Every OLED issue found on this firmware - a dead animation
 * overwriting the menu, a screensaver drawing a black rectangle over the whole
 * screen - showed up as this count dropping to zero.
 */
size_t oled_screen_ink(void);
```

Exposed via the `display` console command (`cmd_control.c:152-167`):
`"Report OLED frame buffer ink; 'display redraw' re-renders"`.

---

## 12. New App: RF Spectrum Analyzer

### 12.1 Overview

Added in commit `f85b1a53` (13 files, +2,003 lines). An application that
**monitors the 2.4 GHz spectrum** and visualizes it across four modes.

| File | Lines | Purpose |
|---|---|---|
| `rf_spectrum.c/.h` | 609/158 | Promiscuous RX callback, sweep task, input handling, JSON export |
| `rf_spectrum_channel.c` | 36 | Channel range helpers (host-testable) |
| `rf_spectrum_drone.c/.h` | 123/45 | Passive OpenDroneID / Remote ID detector |
| `rf_spectrum_pcap.c/.h` | 142/60 | Standard PCAP capture to microSD |
| `rf_spectrum_screens.c/.h` | 322/51 | OLED renderer (bars / scope / waterfall / foxhunt) |
| `rf_spectrum_web.c/.h` | 219/40 | HTTP dashboard + JSON status API + control API |

### 12.2 The Four View Modes

```c
typedef enum {
  SPECTRUM_VIEW_BARS = 0,      /* 14 Wi-Fi channels, bar graph */
  SPECTRUM_VIEW_SCOPE = 1,     /* single channel, live power trace */
  SPECTRUM_VIEW_WATERFALL = 2, /* scrolling 2D waterfall */
  SPECTRUM_VIEW_FOXHUNT = 3    /* high-gain proximity meter */
} spectrum_view_mode_t;
```

Constants: `RF_WIFI_MAX_CHANNELS 14`, `RF_ZIGBEE_MIN_CHANNEL 11`,
`RF_ZIGBEE_MAX_CHANNEL 26`, `RF_NOISE_FLOOR -95`, `RF_MAX_SIGNAL -25`.

### 12.3 Features

| Feature | Implementation |
|---|---|
| Per-channel RSSI with peak-hold | Promiscuous RX callback updates `channel_rssi[]`, `channel_peak[]`, `channel_packets[]` |
| Scope | 128-sample ring buffer for the focused channel |
| Waterfall | 36 rows x 27 channels, advancing one row every 3 ticks (~60 ms) |
| Fox hunting | `Prox% = (RSSI + 95) / 70 * 100` |
| **RF IDS** | Frame control byte 0: `0xC0` = deauth, `0xA0` = disassoc |
| **Jammer detection** | RSSI >= -45 dBm for 20 consecutive sweeps triggers `jammer_alert[ch]` |
| **OpenDroneID** | IE 221 with OUI `FA:0B:BC`, French Remote ID `6A:5C:35`, NAN Action Frames `88:69:19:9D:92:09` |
| **Audio Geiger counter** | Buzzer click rate scales with signal strength |
| **CSV logging** | `/sdcard/spectrum_log.csv` |
| **PCAP capture** | `/sdcard/spectrum_NN.pcap`, link type 105 (IEEE 802.11) |
| Band switching | Wi-Fi <-> Zigbee/802.15.4 |

### 12.4 Capture Loop

The `rf_spectrum_task` (stack 4096, priority 5) runs every 20 ms:

1. **Tune**: stays on the selected channel in SCOPE and FOXHUNT modes, or
   sweeps channels 1 through 14 in BARS and WATERFALL modes. Calls
   `wifi_driver_set_channel()` (not `esp_wifi_set_channel()` directly) for the
   reason explained in `50496e10`: avoiding redundant channel switches prevents
   unnecessary radio resets and ~37-40 KB buffer allocations.
2. Updates jammer streaks, decays alerts, pushes to the scope ring buffer,
   advances a waterfall row every 3 ticks, smooths RSSI (1 dB decay per tick,
   peaks every 20 ticks), flushes CSV every 50 ticks, and renders the screen.

Promiscuous mode is acquired via `radio_lease`:

```c
  if (radio_lease_prom_acquire(RADIO_PROM_SPECTRUM, spectrum_promiscuous_rx_cb) != ESP_OK) {
    ESP_LOGW(TAG, "Could not claim promiscuous mode");
  }
```

### 12.5 Web Dashboard

```c
/* rf_spectrum_web.h:10-15 */
/* Not 80: the captive portal (main/apps/wifi/captive) and the File Manager web
   browser (main/modules/web_file_browser) both bind the default HTTP port, and
   Wi-Fi is up whenever the spectrum app runs. Sharing 80 made httpd_start()
   fail and the dashboard silently unavailable. */
#define RF_SPECTRUM_WEB_PORT 8080
#define RF_SPECTRUM_WEB_CTRL_PORT 32769
```

| Route | Method | Purpose |
|---|---|---|
| `/spectrum` | GET | Interactive web dashboard (inline HTML/CSS/JS) |
| `/api/spectrum` | GET | JSON state snapshot |
| `/api/spectrum/control` | POST | `?th=<dBm>`, `?cmd=audio\|log\|proto` - **requires admin header** |

Admin header protection:

```c
/* rf_spectrum_web.h:17-21 */
/* Custom header the /api/spectrum/control endpoint requires. A request that
   carries it is a CORS "preflighted" request, which a browser only sends for
   same-origin calls from a page we served. Without this, any page on the
   network could tamper with the running RF Spectrum session. */
#define RF_SPECTRUM_ADMIN_HDR "X-Minino-Admin"
```

The dashboard polls `/api/spectrum` every 250 ms and renders a live bar chart
on an HTML5 `<canvas>`.

### 12.6 Console Commands

| Command | Help Text |
|---|---|
| `threshold` | `Set or view RF Spectrum RSSI threshold filter (dBm or 'off')` |
| `spectrum_audio` | `Control RF Spectrum audio Geiger feedback (on, off, toggle)` |
| `spectrum_log` | `Control RF Spectrum MicroSD CSV logging (on, off, toggle)` |
| `spectrum_pcap` | `Control RF Spectrum standard PCAP capture to MicroSD (on, off, toggle)` |
| `spectrum_band` | `Switch RF Spectrum band (wifi, zigbee)` |
| `spectrum_ids` | `View or clear RF IDS deauth & jammer alerts (clear)` |
| `spectrum_drone` | `View or clear DroneID / Remote ID detections (clear)` |

---

## 13. Buttons: Dedicated Queue and Task

### 13.1 The Problem

`iot_button` polls GPIO pins from an `esp_timer` created with
`dispatch_method = ESP_TIMER_TASK`. Consequently, button callbacks executed on
the **`esp_timer` task** (priority 22, 3,584-byte stack), which services
**every** software timer in the system.

From `button_event_cb()`, execution flowed directly into `menus_input_cb()` and
into app `on_enter_cb` / `select_cb` handlers. Many of those handlers perform
heavy blocking operations: `sd_card_mount()`, `nvs_flash_init()`,
`esp_openthread_init()`, multi-second `vTaskDelay()` calls, or
`esp_openthread_lock_acquire(portMAX_DELAY)`. While they blocked, button
debounce timers, LED effects, screensaver and AFK timers, the heap monitor, and
Wi-Fi timers **all stalled**. Mounting the SD card from a button press froze
the entire UI until mounting completed.

### 13.2 The Solution

```c
#define INPUT_QUEUE_LEN 16
#define INPUT_TASK_STACK 4096
#define INPUT_TASK_PRIORITY 4

typedef struct {
  uint8_t button_name;
  uint8_t button_event;
} input_event_t;
```

| Parameter | Value |
|---|---|
| Queue length | 16 events (2 bytes each) |
| Task | `"input_task"`, stack 4096, **priority 4** |
| Send timeout | **0** (never blocks the timer task) |
| Dropped event counter | `s_dropped_events`, logs a warning every 16 drops |

The producer (`button_event_cb`) simply unpacks the event and enqueues it
without logging or invoking callbacks:

```c
  input_event_t ev = {.button_name = button_name, .button_event = button_event};
  /* Zero timeout: never block the esp_timer task. A full queue means an app
     callback is still running; dropping is better than stalling every timer in
     the system. */
  if (xQueueSend(s_input_queue, &ev, 0) != pdTRUE) {
    s_dropped_events++;
    if ((s_dropped_events % 16) == 1) {
      ESP_LOGW(TAG, "input queue full, dropped %u event(s) so far", (unsigned) s_dropped_events);
    }
  }
```

The consumer (`input_task`) dequeues, logs, and dispatches:

```c
  input_event_t ev;
  while (true) {
    if (xQueueReceive(s_input_queue, &ev, portMAX_DELAY) != pdTRUE) continue;
    ESP_LOGI(TAG, "Button %s event %s", button_to_name[ev.button_name], event_to_name[ev.button_event]);
    /* ... read cb and locked state under mutex ... */
    if (locked || cb == NULL) continue;
    cb(ev.button_name, ev.button_event);
  }
```

### 13.3 New API Functions

```c
uint32_t keyboard_module_dropped_events(void);   /* dropped event count */
bool keyboard_module_inject_event(uint8_t button, uint8_t event);
```

`keyboard_module_inject_event()` injects a synthetic button event **through the
exact same pipeline** as a physical button press, from the `iot_button`
callback down to the active `input_callback`. This enables driving UI
navigation from the console (`press <button> [event]`) without physical button
presses, allowing automated testing of screensaver wake-up and menu rendering.

### 13.4 Ripple Effect on Priorities

Moving button callbacks from priority 22 down to priority 4 meant that
animation tasks running at priority 5 could now preempt menu rendering. That
exposed the screensaver race fixed in section 9.4.

---

## 14. microSD: Mounting, Slots, and Accurate Error Reporting

### 14.1 State Machine

State is held in five file-scope variables (`sd_card.c:50-55`):

```c
sdmmc_card_t* card = NULL;
static bool _format_if_mount_failed = false;
static sd_card_info_t _sd_card_info;
static bool s_sd_card_mounted = false;
static SemaphoreHandle_t s_sd_mutex = NULL;
```

```
  sd_card_begin()               -> registers "sdtest" command, creates mutex
         │
         ▼
   UNMOUNTED ──sd_card_mount()──►  mount()
                                     ├ spi_bus_initialize()      -> returns real error
                                     ├ esp_vfs_fat_sdspi_mount() -> claims a FATFS slot
                                     │    └ on failure: esp_vfs_fat_unregister_path()
                                     └ s_sd_card_mounted = true
         │                                     │
         │                              ESP_OK  │  error
         │                                     ▼
         │                                 MOUNTED
         │                                     │
         └─────────── sd_card_unmount() ◄──────┘
```

`mount()` is the internal low-level worker; `sd_card_mount()` is the idempotent
public entry point.

### 14.2 The Five Fixes

**a) Reporting the actual error when the SPI bus fails to initialize** (`d5deb48d`)

```c
  ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    /* Report the real error: the caller (and the log) used to see
       ESP_ERR_NO_MEM, which sent the hunt for a memory problem that was
       not there. */
    ESP_LOGE(TAG, "Failed to initialize bus: %s", esp_err_to_name(ret));
-    return ESP_ERR_NO_MEM;
+    return ret;
  }
```

**b) Releasing the VFS FAT slot on mount failure** (`a3e1809c`)

```c
  if (ret != ESP_OK) {
    /* Give the file system slot back: a mount that fails half way leaves the
       context registered, and the next attempt then reports a bogus
       ESP_ERR_NO_MEM ("no free context") instead of the real error. */
    esp_vfs_fat_unregister_path(MOUNT_POINT);
```

**c) Preventing duplicate mounts from leaking FATFS contexts** (`c4f07def`)

```c
esp_err_t sd_card_mount() {
  /* Mounting twice leaves a second FATFS context and a second set of buffers
     behind: the modules that save to the card (the trackers, the thread
     sniffer, the analyzer) called this without asking, and the heap ran out
     after a few of them. The card is already there: use it. */
  if (sd_card_is_mounted()) {
    return ESP_OK;
  }
```

And the per-mount heap footprint was reduced:

```c
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = _format_if_mount_failed,
-     .max_files = 4,
-     .allocation_unit_size = 16 * 1024};
+     .max_files = 2,
+     .allocation_unit_size = 4 * 1024};
```

`max_files` directly scales ESP-IDF's heap allocation
(`sizeof(vfs_fat_ctx_t) + max_files * sizeof(FIL)` plus per-file state). All
**22 call sites** to `sd_card_mount()` now go through this idempotent guard.

**d) The `sdtest` console command** (`161643b6`)

```c
static int sdtest(int argc, char** argv) {
  if (sd_card_is_mounted()) {
    printf("sd_card: already mounted at %s\n", MOUNT_POINT);
    return 0;
  }
  esp_err_t err = sd_card_mount();
  if (err == ESP_OK) {
    printf("sd_card: mounted at %s\n", MOUNT_POINT);
    return 0;
  }
  printf("sd_card: mount failed: %s\n", esp_err_to_name(err));
  return 1;
}
```

**e) Mounting before the first SDR capture** (`9cabe192`)

```c
  /* The card is mounted before the first capture, not after it: the capture
     leaves the analog front end in a state the SD bus does not always
     answer to (the mount then fails with "command not supported" and the
     file system slots stay occupied, so the next attempt reports a bogus
     ESP_ERR_NO_MEM). */
  if (!sd_card_is_mounted()) {
    esp_err_t err = sd_card_mount();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "microSD not available (%s): the captures are not saved", esp_err_to_name(err));
    }
  }
```

### 14.3 `flash_fs`: Recursive Mount Mutex Fix

> **Attribution note.** Commit `b8346169` ("fix: recursive mount mutex
> preventing first-mount hang") applies to `firmware/components/flash_fs/flash_fs.c`
> (SPIFFS), **not** `sd_card`. The `sd_card` mutex is only taken around file
> operations.

---

## 15. BLE: HID, GATT, Spam, and GAP Synchronization

### 15.1 BLE HID: Asynchronous Deinit

**The problem.** `esp_hidd_profile_deinit()` previously called
`memset(&hidd_le_env, 0, ...)` immediately. However, Bluedroid deinitialization
is **asynchronous**: the call merely enqueues teardown work on the BTC task.
Any `GATTS_WRITE` or `GATTS_CONFIRM` event arriving in the meantime hit NULL
pointers inside `hidd_le_env` and crashed the BTC task.

```c
/* after - esp_hidd_prf_api.c:92-110 */
void esp_hidd_wait_for_unreg(uint32_t timeout_ms) {
  int timeout = (int) (timeout_ms / 10);
  while (!hidd_le_env.unreg_signaled && timeout-- > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!hidd_le_env.unreg_signaled) {
    ESP_LOGW(HID_LE_PRF_TAG, "app_unregister completion not observed");
  }
  /* The registration is gone (or timed out): nothing will call hidd_cb
     through this profile any more, so the environment can be wiped. */
  memset(&hidd_le_env, 0, sizeof(hidd_le_env_t));
}
```

And the updated `ble_hid_end()`:

```c
void ble_hid_end(void) {
  if (!s_hid_started) return;
  s_hid_started = false;
  esp_ble_gap_stop_advertising();
  /* If a host is still connected, close the ACL first: deleting the service
   * and unregistering the app under a live link produces disconnect events on
   * an already-deleted profile and wedges the next enable. */
  if (is_connected && hidd_le_env.gatt_if != ESP_GATT_IF_NONE) {
    esp_ble_gatts_close(hidd_le_env.gatt_if, hid_conn_id);
  }
  gap_dispatcher_unregister(gap_event_handler);
  esp_hidd_profile_deinit();
  /* profile_deinit() only queues stop/delete/unregister on the BTC task: the
   * stack must not be torn down until the queued work has run, otherwise a
   * second enable deadlocks on a half-registered SRS. */
  esp_hidd_wait_for_unreg(500);
  ...
  radio_lease_ble_release();
  radio_lease_ble_unblock_scan();
}
```

**Out-of-bounds / uninitialized LED write bug** (`hid_device_le_prf.c:662-681`):

```c
-      if (param->write.handle ==
-          hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL]) {
+      if (hidd_le_env.hidd_cb != NULL &&
+          hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL] != 0 &&
+          param->write.handle ==
+              hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL]) {
```

Two bugs on one line: `hidd_cb` could be **NULL**, and `att_tbl[...]` could be
**0** (handle 0 is never a valid attribute handle, so a write to handle 0
matched the uninitialized entry and invoked the callback with a bogus LED write
payload).

**`hidd_clcb_dealloc()` clearing the wrong connection slot:**

```c
/* before - returned true on the first iteration without checking conn_id */
-  for (i_clcb = 0, p_clcb = ...; i_clcb < HID_MAX_APPS; i_clcb++, p_clcb++) {
-    memset(p_clcb, 0, sizeof(hidd_clcb_t));
-    return true;
+  for (p_clcb = ...; p_clcb < ... + HID_MAX_APPS; p_clcb++) {
+    if (p_clcb->in_use && p_clcb->conn_id == conn_id) {
+      memset(p_clcb, 0, sizeof(hidd_clcb_t));
+      return true;
+    }
   }
```

**Connection state notification.** Previously, `ESP_HIDD_EVENT_BLE_CONNECT`
notified the UI immediately before bonding completed, causing consumer control
reports sent right away to be dropped silently. Connection notification now
waits for authentication completion:

```c
      if (param->ble_security.auth_cmpl.success) {
        sec_conn = true;
        is_connected = true;
        if (hid_event_callback != NULL) hid_event_callback(true);
      } else {
        /* A failed pairing usually means our bond for this peer is stale (the
           classic HOGP "PIN or Key Missing"): the host keeps its copy of the
           key, must not be asked to pair afresh while we hold the old record.
           Drop our side so the next attempt is a clean pairing, sparing the
           user the manual "forget device" dance. */
        hid_drop_bond(bd_addr);
        if (hid_event_callback != NULL) hid_event_callback(false);
      }
```

And all consumer control actions now pass through a single gate that reports
whether the host actually received the report:

```c
/* Every action button funnels through here so that "did the host actually get
   the report?" has exactly one answer. The UI needs it: the notification
   windows used to be drawn unconditionally, so pressing Volume Up with no host
   attached flashed a confirmation for something that was silently dropped. */
static bool hid_send_consumer(const char* action, uint16_t key_cmd, bool press);
```

### 15.2 BLE Spam GAP Synchronization

**The problem** (`bt_spam_gap_sync.h:1-15`):

> Bluedroid reports four different completion events through one binary
> semaphore (ADV_STOP, SET_STATIC_RAND_ADDR, ADV_DATA_RAW_SET, ADV_START).
> Returning "done" for whichever token happens to wake the waiter desynchronises
> the cycle: a single stale token shifts every later wait by one step, so
> `is_advertising_active` no longer reflects the controller and the loop issues
> stop/configure/start without confirmation.

**The solution:** a pure C module (no FreeRTOS dependency) that tracks whether a
semaphore wakeup matches the specific GAP command that was issued:

```c
typedef struct {
  uint32_t completed;                          /* monotonic counter */
  esp_gap_ble_cb_event_t last_event;
  esp_bt_status_t last_status;
} bt_spam_gap_sync_t;

void     bt_spam_gap_sync_reset(bt_spam_gap_sync_t*);
bool     bt_spam_gap_sync_complete(bt_spam_gap_sync_t*, esp_gap_ble_cb_event_t, esp_bt_status_t);
uint32_t bt_spam_gap_sync_mark(const bt_spam_gap_sync_t*);   /* snapshot */
bool     bt_spam_gap_sync_has_new(const bt_spam_gap_sync_t*, uint32_t mark);
bool     bt_spam_gap_sync_matches(const bt_spam_gap_sync_t*, esp_gap_ble_cb_event_t wanted);
bool     bt_spam_gap_sync_succeeded(const bt_spam_gap_sync_t*);
```

**Mandatory semaphore drain ordering:**

```c
/* Drop every token currently posted. Must be called *before* issuing an
   asynchronous GAP command, never after: btc_transfer_context() only blocks
   until BTC_TASK dequeues the message, so the completion event has not been
   delivered yet and a drain afterwards is a no-op. */
static inline void drain_sem(void);
```

Covered by 13 host unit tests, including
`test_every_out_of_order_permutation_resolves` (testing all 8 permutations x 4
targets).

### 15.3 Advertising Payload Validation

```c
/* A malformed advertising payload is worse than a missing one: a wrong AD
   length byte or a .len that disagrees with the byte count makes the peer's
   parser discard the whole advertisement, so the model silently never fires.
   Both mistakes are invisible in the struct itself (the compiler only warns
   about an overflowing initializer), which is why the table is checked here
   once per start instead of being trusted. */
static bool ad_payload_is_well_formed(const spam_model_t* model) {
  if (model->len == 0 || model->len > sizeof(model->data)) return false;
  uint8_t i = 0;
  while (i < model->len) {
    uint8_t ad_len = model->data[i];
    if (ad_len == 0) return false;
    if ((uint32_t) i + 1u + ad_len > model->len) return false;
    i = (uint8_t) (i + 1u + ad_len);
  }
  return i == model->len;
}
```

### 15.4 Advertising Exclusivity via the Scan Gate

```c
  /* Spam needs the radio completely idle before it can work at all: the gate
   * is taken *before* stopping the scan so no scanner's restart loop re-arms
   * a scan behind our back (the restart loops of the bt_gattc consumers check
   * radio_lease_ble_scan_allowed() before re-arming). */
  radio_lease_ble_block_scan();
```

This prevents the controller errors:

```
"Advertising or scanning now, can't set randaddress"
"No random address yet, please set random address and try"
```

---

## 16. Build System: Variants, Kconfig, and Capture Bank Cap

### 16.1 The Two-Pass CMake Evaluation Problem

ESP-IDF evaluates each component's `CMakeLists.txt` **twice**:

1. During component tree expansion - **before** Kconfig runs, when no
   `CONFIG_*` variables exist yet.
2. During build configuration - after Kconfig runs.

Using a plain `if(CONFIG_FOO)` takes the false branch on the first pass,
registering the component **with no source files and no `REQUIRES` list**. That
causes cryptic build failures on the second pass:

```
bt_gattc.h includes esp_bt.h ... not in the requirements list of bt_gattc
```

### 16.2 The `minino_kconfig_enabled()` Helper

`firmware/cmake/minino_kconfig.cmake` (40 lines, added in `8d4173ce`):

```cmake
function(minino_kconfig_enabled OPTION RESULT)
    set(${RESULT} 0 PARENT_SCOPE)
    if(NOT DEFINED SDKCONFIG OR NOT EXISTS "${SDKCONFIG}")
        return()
    endif()
    file(STRINGS "${SDKCONFIG}" _hits REGEX "^CONFIG_${OPTION}=")
    foreach(_hit IN LISTS _hits)
        if(_hit MATCHES "=y$")
            set(${RESULT} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()
```

Because `sdkconfig` (and `SDKCONFIG_DEFAULTS`, see section 23.5) is accessible
during both passes, reading it directly provides consistent component
registration across passes.

Usage:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(BT_ENABLED _minino_opt_on)
if(_minino_opt_on)
    idf_component_register(SRCS ...)
else()
    idf_component_register()
endif()
```

### 16.3 Conditionally Excluded Components

| Component | Controlling Symbol |
|---|---|
| `ble_hid`, `ble_scann`, `bt_gattc`, `bt_gatts`, `bt_spam`, `drone_id`, `uart_sender` | `BT_ENABLED` / `MININO_BT_STACK` |
| `esp-zigbee-console`, `zb_cli`, `zigbee_light`, `zigbee_switch` | `MININO_ZIGBEE_STACK` |
| `open_thread`, `thread_broadcast` | `MININO_OPENTHREAD_STACK` |
| `gap_dispatcher`, `radio_lease` | Source-level `#if CONFIG_BT_ENABLED` |

In `firmware/main/CMakeLists.txt`, `main` application sources are filtered by
path:

```cmake
if(NOT _minino_bt_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/ble/")
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/drone_id/")
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/droneid_scanner/")
endif()
if(NOT _minino_zigbee_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_bee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/settings/zb_cli_settings/")
endif()
if(NOT _minino_openthread_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/thread_sniffer/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/open_thread/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_thread/")
endif()
```

And host unit tests are excluded from the firmware build:

```cmake
foreach(file ${ALL_SOURCE_FILES})
    if(NOT file MATCHES "/test/")
        list(APPEND modules_src ${file})
    endif()
endforeach()
```

> **CMake pitfall.** `firmware/main/CMakeLists.txt:57-60` warns **never** to
> add `REQUIRES` or `PRIV_REQUIRES` to `main`: `main` relies on ESP-IDF's
> default behavior (implicitly requiring all project components), and naming a
> single requirement disables that default and drops all other components from
> the include path.

### 16.4 Kconfig Options

| Symbol | Type | Kconfig Default | Effective Default | Where Used |
|---|---|---|---|---|
| `SDR_ENABLE` | bool | `n` | **`y`** in SDR profile (`n` in board profiles) | `main/CMakeLists.txt:87`, `esp_sdr_memory.c:38`, `menus_data.c:644` |
| `MININO_ZIGBEE_STACK` | bool | `y` | `y` | `main/CMakeLists.txt:16,26-31` |
| `MININO_OPENTHREAD_STACK` | bool (`depends on MININO_ZIGBEE_STACK`) | `y` | `y` | `main/CMakeLists.txt:17,32-36` |

The `MININO_` prefix is intentional: a Kconfig symbol cannot be defined in two
places, and `OPENTHREAD_ENABLED` is already defined by upstream OpenThread.

The `SDR_ENABLE` help text documents the mechanism:

> The engine needs the whole of SRAM block 2 (0x40840000) as its capture
> bank, so the application image has to fit below it: the build caps the SRAM
> flow segment at the block boundary and reserves the block from the heap. If
> the image does not fit, the link fails with "SDR: the DRAM image overlaps
> the capture bank" instead of corrupting the heap at capture time.

### 16.5 `sdkconfig.defaults`: IRAM Trimming

```diff
+# The SDR build variant needs the whole of SRAM block 2 (0x40840000) as the
+# capture bank, so the application image has to fit below it. Moving the Wi-Fi
+# and PHY hot code out of IRAM gives back 45 KB of SRAM, the same trade the
+# esp-sdr project makes ("Leave enough non-modem SRAM for static data and
+# task stacks"). The Wi-Fi gets a little slower; nothing changes functionally.
+# CONFIG_ESP_WIFI_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_EXTRA_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_SLP_IRAM_OPT is not set
+# CONFIG_ESP_PHY_IRAM_OPT is not set
+# CONFIG_PM_SLP_IRAM_OPT is not set
+
+# The SDR app: the capture bank is reserved and the image is capped below it.
+CONFIG_SDR_ENABLE=y
```

> **`# CONFIG_X is not set` is not a passive comment**: it is kconfgen syntax
> to explicitly disable an option. Because `CONFIG_PM_SLP_IRAM_OPT` was set to
> `=y` on line 10 and `"is not set"` on line 160, the later line wins in
> `sdkconfig.defaults`.

### 16.6 Vendored `vrt_packet`

Previously, `vrt_packet` was compiled from an external `esp-sdr` checkout
populated by `tools/patches/apply_esp_sdr_minino.py` into `<repo>/build-sdr`.
Because `build-sdr` is gitignored and the patch script was removed, clean
builds silently degraded to an empty component. The sources are now vendored
directly in the repository:

```
firmware/components/vrt_packet/
├── iq_pack.c   163
├── iq_pack.h    64
├── vrt_packet.c 439
└── vrt_packet.h 313
```

### 16.7 Partition Tables

Only `partitions_no_ota.csv` was modified:

```diff
-internal,   data,  spiffs,         ,     4248K,
-factory,    app,  factory,         ,     3200K,
+internal,   data,  spiffs,         ,     4248K,
+factory,    app,  factory,         ,     3700K,
```

| Partition Table | Change |
|---|---|
| `partitions.csv` (8 MB, OTA) | Unchanged: `ota_0` and `ota_1` at 3700K |
| `partitions_4MB.csv` | Unchanged: `factory` at 3400K |
| `partitions_no_ota.csv` | `factory` 3200K -> **3700K** (+500K) |
| `partitions_8MB_NO.csv` | Unused by any profile; remains at 3200K |

### 16.8 Makefile Targets

```make
FLASH_FIT_BOARDS_4MB = bsides dragonjar ekoparty bugcon bsseattle

flash_fit:
	@python3 tools/check_flash_fit.py --partition-table partitions.csv \
		--app-bin build-minino/minino.bin --label minino || exit 1; \
	for board in $(FLASH_FIT_BOARDS_4MB); do \
		python3 tools/check_flash_fit.py --partition-table partitions_4MB.csv \
			--app-bin build-$$board/minino.bin --label $$board || exit 1; \
	done
```

And the test runner target:

```make
tests:
	$(MAKE) -C components/surveillance_detect/test run-tests
	$(MAKE) -C components/nmea_parser/test run-tests
	$(MAKE) -C components/drone_id/test run-tests
	$(MAKE) -C components/files_ops/test run-tests
	$(MAKE) -C components/gap_dispatcher/test run-tests
	$(MAKE) -C components/radio_lease/test run-tests
	$(MAKE) -C components/event_bus/test run-tests
	$(MAKE) -C components/bt_spam/test run-tests
	$(MAKE) -C main/modules/resistor_detector/test run-tests
	$(MAKE) -C main/modules/cmd_control/test run-tests
	$(MAKE) -C main/modules/esp_sdr/test run-tests
	$(MAKE) -C main/modules/menus_module/test run-tests
	$(MAKE) -C main/apps/ble/hid_device/test run-tests
	$(MAKE) -C main/apps/rf_spectrum/test run-tests
	$(MAKE) -C main/general/general_scrolling_text/test run-tests
	python3 tools/test/test_flash_tools.py
	python3 tools/test/test_sdr_patch.py
```

> **Note:** the final line originally invoked `tools/test/test_sdr_patch.py`,
> which was deleted in `fe1e0223` and subsequently removed from the Makefile
> (see section 22.3).

`flash_fit` is chained into the release build script
(`firmware/get_builds.sh:23-24`):

```bash
make builds
make flash_fit
make mergebins
```

---

## 17. Host Tests and CI

### 17.1 Why Host Tests

Testing exclusively on hardware requires flashing, booting, triggering the
feature, and inspecting serial output, which does not scale to every commit.
Host tests compile with native `gcc` on the workstation against lightweight
ESP-IDF header mocks.

Three rules make this work:

1. **Code under test must not depend on ESP-IDF runtime headers.** That is why
   `bt_spam_gap_sync.c`, `radio_lease_registry.c`, `event_bus.c`,
   `rf_spectrum_channel.c`, and the `resistor_detector` math routines are
   factored into standalone files.
2. **Each suite provides its own minimal mocks.** The `test/mock/` directories
   provide stub headers such as `esp_err.h`, `esp_log.h`,
   `freertos/FreeRTOS.h` (implementing spinlocks as simple counters),
   `esp_gap_ble_api.h`, and `sdkconfig.h`.
3. **Unity** is sourced from ESP-IDF (`$IDF_PATH/components/unity/unity/src`),
   except for `files_ops/test/unity/`, which vendors Unity directly in-tree.

### 17.2 The 16 Test Suites

| Suite | Tests | Invariants Verified |
|---|---|---|
| `components/surveillance_detect/test` | 80 | Signatures, matching, IEs, overlay, detection engine, wardriving |
| `components/nmea_parser/test` | 22 | Manual NMEA sentence decoding API |
| `components/drone_id/test` | 27 | Altitude, Unix timestamp, OpenDroneID, spoofing, UTM |
| `components/files_ops/test` | 23 | Overflow-safe sizes, path traversal protection, incremental naming |
| `components/gap_dispatcher/test` | 9 | Ensuring later GAP registrations do not starve earlier subscribers |
| `components/radio_lease/test` | 12 | Promiscuous frame fan-out and atomic reference counting |
| `components/event_bus/test` | 11 | Topic isolation and callback re-entrancy |
| `components/bt_spam/test` | 13 | GAP completion event synchronization |
| `main/modules/resistor_detector/test` | 12 | Divider arithmetic and boundary conditions |
| `main/modules/cmd_control/test` | 16 | Console command argument parsing |
| `main/modules/esp_sdr/test` | 23 (+24) | osmoSDR protocol, I/Q packing, and ESPARGOS burst protocol |
| `main/modules/menus_module/test` | 13 | Menu catalog integrity across build variants |
| `main/apps/ble/hid_device/test` | 11 | BLE HID profile lifecycle and bounds |
| `main/apps/rf_spectrum/test` | 11 | Wi-Fi and Zigbee channel validation |
| `main/general/general_scrolling_text/test` | 12 | Scrolling text line-wrapping splitter |
| `firmware/tools/test/test_flash_tools.py` | 5 | Partition flash-fit checker |

**Total: 15 C suites + 1 Python suite.** All suites compile and pass (389 test
assertions total across all variants; see section 22.7).

### 17.3 `gap_dispatcher` as the Canonical Example

Its test header explains the motivation:

> Bluedroid keeps a single GAP callback registered via
> `esp_ble_gap_register_callback()`. If multiple modules call that API, each
> call overwrites the previous one and prior modules silently miss events
> (which is a common cause of "BLE scanning/ADV stopped working").

The mock (`test/mock/esp_bt_main.c`) reproduces Bluedroid's exact contract:
`esp_ble_gap_register_callback()` **rejects** calls when the stack is not
`ENABLED` and stores **only a single** callback pointer (last caller wins).

Key test cases:

| Test | What It Guards |
|---|---|
| `test_registering_later_does_not_starve_earlier_subscribers` | **The exact bug the component was created to prevent** |
| `test_registering_after_a_bluedroid_restart_reclaims_the_slot` | After a disable/enable cycle, the next registration reclaims the Bluedroid slot |
| `test_registering_twice_does_not_duplicate` | Spam and scanner modules re-enter `begin()` on every menu visit |
| `test_subscriber_table_is_bounded_and_reports_overflow` | Table is bounded and overflow does not evict existing subscribers |

### 17.4 `menus_module` Compiled Across Multiple Variants

```make
build/run-full build/run-no-sd build/run-no-bluetooth build/run-no-wifi build/run-no-stacks
```

Reduced variants catch "orphan child" bugs that a full build hides: if a child
menu entry is compiled without matching its parent's `#if` gate,
`navigation_exit()` lands on an unresolvable parent ID.

`mock/sdkconfig.h` (99 lines) substitutes for the generated `sdkconfig` using
`TEST_CONFIG_<FEATURE>_ENABLED` overrides.

### 17.5 Continuous Integration

`.github/workflows/host-tests.yml` runs on `ubuntu-latest`:

```yaml
      - name: Fetch Unity from ESP-IDF
        run: |
          git clone --depth 1 -b release/v5.5 --filter=blob:none --sparse \
              https://github.com/espressif/esp-idf.git "$RUNNER_TEMP/esp-idf"
          git -C "$RUNNER_TEMP/esp-idf" sparse-checkout set components/unity components/console
          git -C "$RUNNER_TEMP/esp-idf" submodule update --init --depth 1 components/unity/unity
```

Each suite runs as a dedicated step with `IDF_PATH: ${{ runner.temp }}/esp-idf`.
Comments on each step document **why** the suite exists:

> # Bluedroid keeps a single GAP callback, so gap_dispatcher is the only
> # reason BLE spam, BLE HID, the scanners and the gatt console commands can
> # all receive GAP events at once. It regressed twice.

> # event_bus cannot carry GAP events; it replaced the GAP registration in
> # four gattcmd handlers and silently did nothing.

> # The BLE spam advertising loop waits on one semaphore fed by four GAP
> # completion events; a stale token desynchronised it and stopped the spam.

> # The scrolling-text splitter wrote one entry past its heap array because
> # its counting and filling passes used different wrap thresholds; every
> # Help/Credits/Legal screen goes through it.

---

## 18. Host Tools

Five new files in `firmware/tools/`.

### 18.1 `check_flash_fit.py` (97 lines)

Parses an ESP-IDF partition table CSV, finds the **smallest** `app` partition,
and verifies that the compiled binary fits with sufficient margin.

```
python3 tools/check_flash_fit.py --partition-table <csv> --app-bin <bin> \
        [--min-free-percent FLOAT] [--label STR]
```

| Option | Default | Description |
|---|---|---|
| `--partition-table` | *required* | Path to partition table CSV |
| `--app-bin` | *required* | Path to compiled app binary |
| `--min-free-percent` | **5.0** | Minimum required free space (%) |
| `--label` | `app` | Prefix for each output line |

Sample output:

```
[minino] partition size: 3700.0 KiB
[minino] image size: 3336.0 KiB
[minino] free: +364.0 KiB (9.84%)
[minino] OK
```

Exit codes: 0 on pass, 1 on failure (`ERROR: Partition table not found`,
`ERROR: App binary not found`, `ERROR: No app/factory partition found in
partition table`, or `FAIL: free space X% below minimum Y%`).

CSV parsing skips comments, rows with fewer than 5 fields, and unparseable
rows; sizes accept `K`/`M` suffixes and hex `0x...` offsets.

### 18.2 `fix_bootloop.sh` (38 lines)

Recovers a board stuck in a bootloop after a bad OTA update. Resets `otadata`
to slot 0 and flashes a known-good Minino image to **both** OTA slots.

```bash
bash firmware/tools/fix_bootloop.sh [PORT]     # default PORT: /dev/ttyACM0
```

| Step | Action | Offset |
|---|---|---|
| 1 | Reset `otadata` to slot 0 | `0x15000` |
| 2 | Flash Minino into slot 1 (clearing any broken image) | `0x440000` |
| 3 | Flash Minino into slot 0 | `0xa0000` |

| Constant | Value |
|---|---|
| Port probe | 30 iterations x 0.5 s (15 s), falling back to `/dev/ttyACM1` |
| Baud rate | 460800 baud |
| Target chip | `esp32c6` |
| Artifacts | `firmware/build/ota_data_initial.bin`, `firmware/build/minino.bin` (relative to repo root) |

### 18.3 `sdr_capture.py` (192 lines)

Drives the ESP-SDR receiver over USB Serial/JTAG: synchronizes, tunes,
requests an I/Q burst, writes raw bytes to a file, and prints signal
statistics.

```
python3 firmware/tools/sdr_capture.py [--port PORT] [--freq MHz] [--count N]
                                      [--rate R] [--out FILE]
```

| Option | Default |
|---|---|
| `--port` | `/dev/ttyACM0` |
| `--freq` | **2400** MHz (100 to 6000) |
| `--count` | **4096** samples |
| `--rate` | **0** (decimation code; 0 = full rate) |
| `--out` | `sdr_capture.bin` |

| Constant | Value |
|---|---|
| Serial port | 115200 baud, `dtr = False`, `rts = False` |
| Sync | 10 attempts sending `TRANSPORT?`, 1 s apart |
| `CAP20` packing | **20 bits per sample pair** (10-bit I, 10-bit Q), little-endian |
| Payload bytes | `count * 20 / 8` |
| I/Q bit depth | 10-bit, centered at 512 |

Protocol sequence: `TRANSPORT?`, `VERSION?`, `LIMITS?`, `FREQ <MHz>` (expects
`OK`), `CAP20 <count> <rate>` -> `DATA <count>` + raw payload + `END`.

Signal validation compares the complex magnitude RMS against its peak: if RMS
is `< 1.0`, it warns *"the burst is flat, the receiver may not be running"*; if
RMS exceeds peak, it exits with code 1. Requires `pyserial`.

### 18.4 `test_live_sdr.py` (119 lines)

Five-step live radio check from the earlier dual-boot stage: detects whether
Minino OS or esp-sdr is running (sending `boot_sdr` if on Minino), opens the
esp-sdr console at **2,000,000 baud**, sends `FREQ 2412` and `CAP20 512 0`,
demodulates 20-bit I/Q, and sends `MININO` to return.

```bash
python3 firmware/tools/test_live_sdr.py [PORT]
```

| Constant | Value |
|---|---|
| `boot_sdr` | 115200 baud |
| SDR console | **2 MBaud** |
| Samples | 512, rate 0, 1,280-byte payload |
| I/Q | 10-bit two's complement, centered at 512 |
| dBFS | `20 * log10(max(1e-6, avg_pwr / 512.0))` |

Renders an ASCII bar graph of the first 12 samples.

> Note: This script uses `boot_sdr`, which was later replaced by `sdr_capture`
> and `sdr_host` (section 4.7). Preserved as a protocol reference.

### 18.5 `test/test_flash_tools.py` (90 lines)

Python `unittest` suite for `check_flash_fit.py` (originally 294 lines when
also covering `check_ota_image.py`, which was removed in `ff3cd744`).

| Test | What It Guards |
|---|---|
| `test_parse_explicit_and_implicit_offsets` | Blank offset (auto-layout) stays 0 rather than being fabricated |
| `test_smallest_app_partition_is_used` | Selects the smallest `app` partition |
| `test_binaries_with_size_suffixes` | `3M` parses to `3 * 1024 * 1024` bytes |
| `test_cli_rejects_tight_fit` | 100K partition, 99K image, `--min-free-percent 5` returns **1** |
| `test_cli_accepts_adequate_room` | 100K partition, 50K image returns **0** |

---

## 19. Minor Changes by Module

### 19.1 `files_ops`: Overflow-Safe Sizes and Path Traversal Protection

`2c177f0d` (348 lines) adds file path validation and safe buffer helpers:

```c
bool files_ops_has_path_traversal(const char* path);
bool files_ops_is_valid_filename(const char* filename);
bool files_ops_sanitize_filename(const char* input, char* output, size_t out_len);
bool files_ops_join_path(const char* dir_path, const char* name,
                         const char* extension, char* out_buf, size_t out_len);
bool files_ops_incremental_name_buf(const char* dir_path, const char* base_name,
                                    const char* extension, char* out_buf, size_t out_len);
size_t files_ops_get_file_size(FILE* file);
char* files_ops_format_size(size_t size);
bool files_ops_format_size_buf(size_t size, char* out_buf, size_t buf_len);
bool files_ops_exists(const char* dir_path, const char* name, const char* extension);
```

Tested by **23 unit tests** using in-tree vendored Unity
(`files_ops/test/unity/`), requiring no `IDF_PATH`.

### 19.2 `nmea_parser`: Manual Decode API

`c6a814c4` extracts NMEA sentence decoding into a standalone API independent of
ESP-IDF, covered by 22 host unit tests.

> **Attribution note.** Commit `045de824` ("explicit source list, single
> julian_day definition; add host tests") modifies **`drone_id`**, not
> `nmea_parser` (`nmea_parser/CMakeLists.txt` is identical to `main`).

### 19.3 `drone_id`: Moving Spoofers to the Heap

OpenDroneID spoofers (one per configured drone) were moved from `.bss` to the
heap, freeing **37.7 KB** of static SRAM needed to fit below the capture bank.
`f67c001b` also removed a duplicate `opendroneid.c`.

### 19.4 `gap_dispatcher` and Other BLE Components

| Component / Commit | Change |
|---|---|
| `ble_scann` | Only arms scanner after GATTC bring-up completes, and respects the scan gate (`029ee8c3`) |
| `bt_gatts` | Silenced unused variable warning (`c4c38379`) |
| `bt_gattc` | Error-safe client task `begin`/`stop` and unique app ID (`de1e3dda`) |
| `2452a801` | Moved BLE scan retry off the BTC task |
| `e1dfac40` | Re-applied IEEE 802.15.4 PIB settings on every driver enable |
| `3d95ad2b` | Fixed message pool static state |

### 19.5 `flash_fs` (SPIFFS)

`b8346169` fixed a recursive mutex lock that hung on first mount when a mount
helper re-entered a path that acquired the same mutex (125 lines changed).

### 19.6 `OTA`

`0beea4cc` reformatted `http_server.c` (1,232 lines, CRLF -> LF) and made
`fw_update_reset` `static`. `wifi_ap.c` defers `esp_wifi_init()` to
`wifi_service` (section 8) and posts `EV_WIFI`.

### 19.7 `ieee_sniffer`

69 lines updated to integrate with `radio_lease` and `gap_dispatcher`.

### 19.8 `radio_selector`

`e2165751`: made `radio_selected_option` file-scope `static` (the public
accessor already existed).

### 19.9 Localization

`8f4a3ec5` ("Feat: Localize Spanish Text and translate to english") and its
merge commit (`996ee3ee`) translated UI strings to English and structured
localization headers.

### 19.10 `general_scrolling_text`

`689c925a`: the scrolling-text line splitter previously wrote **one entry past
the end** of its heap array because its line-counting pass and line-filling
pass used slightly different wrap thresholds. Because all Help/Credits/Legal
screens use this component, a standalone split helper and 12 host unit tests
were added.

### 19.11 Console Command Task Queue

`da81e333` moves console command execution into dedicated tasks so blocking
commands (such as a long ping) do not stall the REPL, and adds 16 argument
parsing unit tests.

---

## 20. Complete Commit Table

### 20.1 September 4 to September 18, 2026

| Commit | Date | Subject |
|---|---|---|
| `8f4a3ec5` | 2026-09-04 | Feat: Localize Spanish Text and translate to english |
| `bed0b16c` | 2026-09-04 | fix: format code for better readability in hid_screens.h, animations_module.c, and screen_saver.c |
| `996ee3ee` | 2026-09-04 | Merge pull request #94 from ElectronicCats/Traasnlate |
| `931260ad` | 2026-09-10 | feat: Harden Wi-Fi sniffing and wardriving for improved stability |
| `093f257b` | 2026-09-17 | fix: Enhance scanner OLED display and data rendering |
| `31a7061f` | 2026-09-18 | Merge pull request #95 from ElectronicCats/Fix_pcap |

### 20.2 October 11 to October 12, 2026 - Menu View Styles

| Commit | Date | Subject |
|---|---|---|
| `82927939` | 2026-10-11 | feat: Add selectable OLED menu view styles |
| `8c485cb1` | 2026-10-12 | feat: Improve OLED menu icon rendering and layout |
| `7effae44` | 2026-10-12 | style: Format menu icon map for readability |

### 20.3 October 19, 2026 - New Core Components and Hardening

| Commit | Subject |
|---|---|
| `d56b9e7a` | feat: add radio lease with atomic BLE refcount, scan gate and host tests |
| `3fbb6d9f` | feat: add GATT event fan-out dispatcher and hook stack-reset |
| `92bc14f9` | feat: add typed fan-out event bus |
| `c2f47398` | feat: add singleton WiFi init service |
| `ad5c94ef` | build: register vrt_packet component |
| `de1e3dda` | fix: error-safe GATT client task begin/stop with unique app id |
| `c4c38379` | chore: silence unused warning in gatts module |
| `18871f04` | fix: async HID deinit, NULL guards, LED write OOB and connect handling |
| `029ee8c3` | fix: only arm scanner after gattc bring-up; respect scan gate |
| `6a078453` | fix: harden tracker timer task lifecycle and start gating |
| `280368ca` | fix: spam task stack, task-delete race and early-return cleanup; add GAP sync host tests |
| `2452a801` | fix: move BLE scan retry off the BTC task |
| `fdf0f955` | refactor: defer wifi init to wifi_service |
| `0beea4cc` | style: reformat http_server; fix wifi_ap timeout |
| `7a9aaeff` | build: wrap raw-frame sanity check instead of -zmuldefs |
| `b8346169` | fix: recursive mount mutex preventing first-mount hang |
| `2c177f0d` | fix: overflow-safe sizes and path traversal check; add host tests |
| `c6a814c4` | feat: manual NMEA decode API for host tests |
| `045de824` | build: explicit source list, single julian_day definition; add host tests |
| `e1dfac40` | fix: apply PIB settings on every driver enable |
| `80d47bbf` | fix: static-ify shared driver symbols |
| `c9579d2b` | fix: non-blocking log sink with correct vsnprintf length |
| `080d8b29` | refactor: static-ify module globals |
| `e2165751` | refactor: make radio selection state private |
| `010e3446` | refactor: make module globals private |
| `3d95ad2b` | fix: fix message pool static state |
| `3321c30f` | refactor: defer wifi init to wifi_service |
| `d84eb372` | refactor: defer wifi init to wifi_service |
| `f9aff880` | refactor: defer wifi init to wifi_service |
| `cdd1b586` | refactor: defer wifi init to wifi_service |
| `52138a31` | refactor: defer wifi init to wifi_service |
| `07bf9bfe` | feat: add SDR launcher Kconfig |
| `9c7599ee` | build: exclude host tests from sources; link librftest for CW TX |
| `449b0144` | fix: restore input handling after HID begin; extract data tables |
| `574dfe5e` | feat: unique GATTC app ids via gatt dispatcher; scan gate checks |
| `37869cc6` | fix: gate adv scanner start and stop on bring-up status |
| `913e2d81` | refactor: align spam module with hardened bt_spam lifecycle |
| `6c958e7e` | refactor: align trackers module with hardened scanner lifecycle |
| `f85b1a53` | feat: add RF spectrum analyzer app |
| `1f2f96b9` | feat: add SDR handover module and dual-slot image checks |
| `1a763db1` | fix: harden uart bridge app lifecycle |
| `049500e5` | feat: subscribe sniffer screens to event bus |
| `fd139212` | refactor: move wifi bitmaps out of header |
| `ca0a422d` | fix: defer wifi init to wifi_service |
| `84879287` | refactor: move deauth data tables out of header |
| `1f86aba6` | fix: guard detector lifecycle |
| `d16af92c` | refactor: move drone bitmaps out of header |
| `f67c001b` | build: drop duplicated opendroneid.c |
| `e73754f7` | fix: harden modbus app data and scenes |
| `f5c2747f` | fix: harden ssid spam lifecycle |
| `5508c825` | feat: wire event bus and wifi service in app_main |
| `d270b3a9` | fix: bounds-check severity and component indexes |
| `f3c3c248` | fix: guard monitor state with a mutex and clamp divisions |
| `2d97d08b` | fix: null-safe task naming and leak on self-delete; add deinit |
| `05c33cbb` | feat: add SH1106 support and probe on init |
| `689c925a` | refactor: add split scroll helper and host tests |
| `56f58e1c` | refactor: move shared bitmaps out of header |
| `f708fd5f` | feat: move menu table to code; add menus data and host tests |
| `181feee8` | feat: queue button events to a dedicated task |
| `da81e333` | test: add cmd_control host tests; queue console tasks |
| `a3bf17f2` | fix: harden web file browser routes |
| `7d4b08f2` | fix: harden settings module lifecycle |
| `a7457dd0` | refactor: align animation task with event bus |
| `e0b79f30` | fix: harden catdos module and commands |
| `b0163862` | fix: harden file manager lifecycle |
| `00a2b83d` | refactor: make gps module state private; harden wardriving |
| `31953a42` | refactor: make modal state private |
| `69427bb5` | refactor: make oled screen state private |
| `339f0dbd` | refactor: make open thread module state private |
| `c69437cc` | test: add resistor detector host tests; harden module |
| `857a17a9` | refactor: harden warbee module lifecycle |
| `6758054b` | refactor: harden warthread module lifecycle |
| `96901229` | refactor: move zigbee bitmaps out of header |
| `d861d64f` | build: add flash-fit and SDR image checkers; grow factory partition |
| `88d13fb7` | ci: extend host-test matrix |
| `47feae79` | docs: document SDR integration and RF spectrum features |
| `fe1e0223` | feat: make Applications SDR a native I/Q capture app, drop slot handover |
| `f4f780e1` | feat: replace boot_sdr console command with sdr_capture |
| `09e3a77d` | fix: gate SDR menu entry only on CONFIG_SDR_ENABLE |
| `0411c22c` | fix: remove SDR reboot action from web dashboard |
| `0813ff1c` | feat: SDR_ENABLE default y for the native capture app |
| `ff3cd744` | build: drop dual-boot flasher and image validators |
| `33e6940a` | docs: rewrite SDR integration for the native in-app port |
| `c4eafa7f` | build: drop sdr_check dual-image target |
| `28cbc76d` | build: add a helper to read Kconfig options from the sdkconfig |
| `8d4173ce` | build: let the components drop out of a build variant |
| `b41d7803` | fix: keep the C6 capture bank out of the DRAM image and the heap |
| `cf580c36` | build: add the sdkconfig of the SDR build variant |
| `012b5253` | docs: explain the SDR build variant and the capture bank |
| `ba55fa02` | build: free 94 KB of SRAM for the capture bank |
| `46d235a1` | build: trim the static data of the apps to fit below the capture bank |
| `7a52b727` | build: shrink the buffers of the file tree, the logs and the SSID list |
| `1df8bc9e` | feat: the SDR app shares the firmware with everything else |
| `5c4b8c5c` | docs: the single firmware, the capture bank and the missing TX |
| `d5deb48d` | fix: report the real error when the SD bus does not come up |
| `e26111bd` | build: describe what SDR_ENABLE does in the configuration |
| `161643b6` | feat: add an sdtest command to mount the card and report what happened |
| `a3e1809c` | fix: give the file system slot back when the mount fails |
| `9cabe192` | fix: mount the microSD before the first capture |

### 20.4 October 20, 2026 - SDR Finalization

| Commit | Subject |
|---|---|
| `510f4195` | build: link the CW tone without the certification-test rxtest machinery |
| `ff9877b8` | feat: provide the two symbols the CW tone needs from the archive |
| `bf66e068` | feat: the SDR transmits again, through the certification-test tone |
| `06d240c3` | docs: the CW tone comes from the certification-test archive, minus the rxtest |
| `42bf30dd` | feat: the driver remembers its channel and reports a failed start |
| `6b313971` | fix: the Wi-Fi driver comes back up after the Bluetooth took the radio |
| `c4f07def` | fix: mounting the card twice left a second file system behind |
| `cca3cc43` | build: two subscribers per topic is enough for the event bus |
| `ddf071ca` | fix: the flock app left the Bluetooth stack up when it exited |
| `50496e10` | refactor: the apps ask the driver for its channel before moving it |
| `7ce2761d` | fix: the flock detector app had no exit callback |
| `9e8c715e` | fix: only the 64 KB the dump engine writes are taken off the heap |
| `bce64f44` | feat: the SDR is receive-only, the tone command is gone |
| `10a808ab` | build: the certification-test archive is not linked any more |
| `a787b665` | docs: the SDR is receive-only and the capture bank is 64 KB |

---

## 21. Uncommitted Changes

At the time of auditing `a787b665`, **two** modifications were present in the
working tree:

```
$ git status
	modified:     firmware/main/modules/esp_sdr/c6_sdr_rx.c
	modified:     firmware/main/modules/esp_sdr/esp_sdr_memory.h
```

### 21.1 Restoring the Capture Bank Size to 128 KiB

```diff
--- a/firmware/main/modules/esp_sdr/esp_sdr_memory.h
+++ b/firmware/main/modules/esp_sdr/esp_sdr_memory.h
@@ -23,7 +23,7 @@ extern "C" {
  * what the BLE controller needs: it asks for one large contiguous block
  * and 64 KiB of slack is the difference between fitting and not.
  */
-#define SDR_CAPTURE_BANK_SIZE 0x10000u
+#define SDR_CAPTURE_BANK_SIZE 0x20000u
```

**Why this change was necessary.** Commit `9e8c715e` had lowered the reservation
from 128 KiB to 64 KiB under the assumption that the upper half of block 2
could safely be used by the heap because the dump engine only writes the first
64 KiB. However, as detailed in section 23.6(e), hardware ownership in
`SRAM_OWNER_REG` applies to the **entire 128 KiB block**: during any capture,
CPU access to the upper 64 KiB stalls until the capture finishes, triggering a
task watchdog reset if Wi-Fi buffers were allocated there. Furthermore,
on-board testing confirmed that BLE scanning works properly with the full
128 KiB (`0x20000u`) reservation.

### 21.2 Explicit FreeRTOS Header Include

```diff
--- a/firmware/main/modules/esp_sdr/c6_sdr_rx.c
+++ b/firmware/main/modules/esp_sdr/c6_sdr_rx.c
@@ -12,6 +12,7 @@
 #include "esp_timer.h"
 #include "esp_wifi.h"
 #include "soc/soc.h"
+#include "freertos/FreeRTOS.h"
 #include "wifi_controller.h"
```

`c6_sdr_rx.c` used FreeRTOS critical section primitives without directly
including `freertos/FreeRTOS.h` (relying on transitive inclusion). Adding the
explicit `#include` fixes compilation across header configurations.

---

## 22. Detected Regressions: Actual State of the Test Suite

> **Updated 2026-10-21.** Issues 22.1 through 22.4 and 22.6 were **fixed** in
> the same commit that added ESPARGOS WebSDR support (section 23). Item 22.5 is
> an environment requirement (`IDF_PATH`) rather than a bug. The original
> diagnostics are preserved below alongside the resolution notes.

### 22.1 `event_bus`: 4 of 11 Tests Failed (`EV_WIFI` and `EV_GPS` Silently Rejected)

> **Fixed.** The invalid-slot sentinel was changed from `EVENT_MAX_SUBS` to
> `EVENT_SLOT_INVALID` (`0xff`), so with 2 subscribers per topic, `EV_WIFI`
> (slot 2) and `EV_GPS` (slot 3) are no longer rejected. All 11 of 11 tests
> pass.

**Root cause.** `_topic_slot()` previously used `EVENT_MAX_SUBS` as the
"invalid slot" sentinel:

```c
// event_bus.c:32-38
static uint8_t _topic_slot(event_topic_t topic) {
  if (topic < 0 || topic >= EV_APP_START) {
    return EVENT_MAX_SUBS;  /* sentinel: rejected by caller */
  }
  uint8_t idx = s_topic_index[topic];
  return (idx < MAX_TOPICS) ? idx : EVENT_MAX_SUBS;
}
```

And callers rejected the result whenever `idx >= EVENT_MAX_SUBS`. When
`EVENT_MAX_SUBS` was lowered to `2` in commit `cca3cc43`, **slot index 2 was a
valid topic slot**:

```c
  s_topic_index[EV_INPUT] = s_next_topic++;   /* 0 */
  s_topic_index[EV_MENU]  = s_next_topic++;   /* 1 */
  s_topic_index[EV_WIFI]  = s_next_topic++;   /* 2  <-- collided with sentinel */
  s_topic_index[EV_GPS]   = s_next_topic++;   /* 3  <-- rejected as >= 2 */
```

As a result, `EV_WIFI` and `EV_GPS` were silently rejected:

```
$ make -C firmware/components/event_bus/test run-tests
test_event_bus.c:74:test_topics_do_not_cross_trigger:FAIL: Expected TRUE Was FALSE
test_event_bus.c:96:test_dispatch_without_init_still_isolates_topics:FAIL: Expected TRUE Was FALSE
test_event_bus.c:131:test_unsubscribe_stops_delivery:FAIL: Expected TRUE Was FALSE
test_event_bus.c:188:test_subscriber_may_reenter_the_bus:FAIL: Expected TRUE Was FALSE
-----------------------
11 Tests 4 Failures 0 Ignored
FAIL
```

Decoupling the sentinel (`EVENT_SLOT_INVALID = 0xff`) from `EVENT_MAX_SUBS`
resolved the collision.

### 22.2 `gap_dispatcher`: Test Suite Failed to Compile

> **Fixed.** Added `mock/sdkconfig.h` defining `CONFIG_BT_ENABLED`. All 9 of 9
> tests pass.

```
$ make -C firmware/components/gap_dispatcher/test run-tests
../gap_dispatcher.c:1:10: fatal error: sdkconfig.h: No such file or directory
    1 | #include "sdkconfig.h"
../gap_dispatcher.h:19:10: fatal error: sdkconfig.h: No such file or directory
   19 | #include "sdkconfig.h"
```

Commit `8d4173ce` added `#include "sdkconfig.h"` to `gap_dispatcher.c/.h` to
gate compilation on `CONFIG_BT_ENABLED`, but the test suite's `mock/` directory
did not yet provide a `sdkconfig.h`.

### 22.3 `make tests` Invoked a Deleted Script

> **Fixed.** Removed the `python3 tools/test/test_sdr_patch.py` line from both
> `firmware/Makefile` and `.github/workflows/host-tests.yml`.

`firmware/Makefile:82` previously ran:

```make
	python3 tools/test/test_sdr_patch.py
```

`tools/test/test_sdr_patch.py` was removed in `fe1e0223` when the external
esp-sdr checkout patcher was retired in favor of the native port.

### 22.4 `menus_module`: Test Suite Failed to Compile

> **Fixed.** Added `surveillance_module_begin_flock()` and
> `surveillance_module_stop()` declarations to `mock/surveillance_module.h`.
> The suite now compiles and passes across all five build variants, including
> `no-stacks`.

```
$ make -C firmware/main/modules/menus_module/test run-tests
../menus_data.c:217:20: error: 'surveillance_module_stop' undeclared here (not in a function)
  217 |      .on_exit_cb = surveillance_module_stop,
```

`menu_callbacks_stub.c:54` defined `surveillance_module_stop`, but
`mock/surveillance_module.h` was missing its declaration.

### 22.5 `surveillance_detect`: Requires `IDF_PATH`

This suite references `$(IDF_PATH)/components/unity/unity/src` rather than a
vendored copy of Unity:

```
$ make -C firmware/components/surveillance_detect/test run-tests
cc1: fatal error: /components/unity/unity/src/unity.c: No such file or directory
```

With `IDF_PATH` exported (`. $IDF_PATH/export.sh`), all 80 tests compile and
pass (matching CI).

### 22.6 `hid_device`: Missing from CI Workflow

> **Fixed.** Added the `hid_device` test step to `.github/workflows/host-tests.yml`.

`firmware/main/apps/ble/hid_device/test` (11 tests) was invoked by
`firmware/Makefile:78` but had been omitted from `.github/workflows/host-tests.yml`.

### 22.7 Test Suite Summary

| Suite | Before Fixes | After Fixes |
|---|---|---|
| `surveillance_detect` | 80/80 (requires `IDF_PATH`) | 80/80 (requires `IDF_PATH`) |
| `nmea_parser` | 22/22 | 22/22 |
| `drone_id` | 27/27 | 27/27 |
| `files_ops` | 23/23 | 23/23 |
| `gap_dispatcher` | Failed to compile | 9/9 |
| `radio_lease` | 12/12 | 12/12 |
| `event_bus` | 4 of 11 failed | 11/11 |
| `bt_spam` | 13/13 | 13/13 |
| `resistor_detector` | 12/12 | 12/12 |
| `cmd_control` | 16/16 | 16/16 |
| `esp_sdr` | 23/23 | **47/47** (23 osmoSDR + 24 burst protocol) |
| `menus_module` | Failed to compile | 13/13 across 5 variants |
| `hid_device` | 11/11 (missing from CI) | 11/11 (in CI) |
| `rf_spectrum` | 11/11 | 11/11 |
| `general_scrolling_text` | 12/12 | 12/12 |
| `tools/test/test_flash_tools.py` | 5/5 | 5/5 |
| `tools/test/test_sdr_patch.py` | Missing file | *Removed from `Makefile` and CI* |

**389 test assertions, all passing.**

### 22.8 Other Documentation Discrepancies Corrected

| Location | Previous Claim | Actual Behavior |
|---|---|---|
| `docs/esp-sdr-integration.md` (early draft) | Files are named `sdrcap_NNNN.iq` | `files_ops.c:320` uses `"%s%02d"`, producing `sdrcap00.iq`, `sdrcap01.iq`, ... |
| `docs/esp-sdr-integration.md` (early draft) | "16 -> 4 per topic, 80 -> 60 topics" | Topics went from 160 -> 140 (`EV_APP_START + 32` -> `EV_APP_START + 12`), and subscribers per topic went to 2 before `event_bus` removal |
| `docs/esp-sdr-integration.md` (early draft) | Reserved bank is 64 KiB | Restored to 128 KiB (`0x20000u`) so CPU accesses to the upper half never stall during capture |
| `README.md:166` | `make -C firmware sdr_check` | Target removed in `c4eafa7f` |
| `firmware/tools/test_live_sdr.py` | Sends `boot_sdr` | Command replaced by `sdr_capture` in `f4f780e1` |
| `firmware/components/wifi_service/wifi_service.c:47-97` | Selects mode via `CONFIG_STA_ENABLE`/`CONFIG_AP_ENABLE` | Those symbols are undefined; always falls back to `WIFI_MODE_STA` |
| `firmware/main/apps/surveillance/CMakeLists.txt:1` | `include("${MININO_KCONFIG_CMAKE}")` | `MININO_KCONFIG_CMAKE` is undefined (file is inert because `main` is a single component) |

---

## 23. Connecting the SDR to the ESPARGOS Web Client

> **Added 2026-10-21.** The ESPARGOS web client
> (<https://espargos.net/espsdr/app/>) is a browser-based spectrum and
> waterfall analyzer that communicates with ESP-SDR firmware over USB. This
> section explains the protocol, the modules that implement it, and the build
> fixes required to link the SDR firmware variant.

### 23.1 The Problem: The Web Client Uses a Different Protocol

Minino already included `osmosdr_serial.c` and `esdr_proto.h` (446 + 132 lines),
which implement the **binary** `ESRL` protocol used by the desktop SoapySDR
driver.

However, the ESPARGOS web application speaks a **different** protocol: the
line-oriented ASCII + binary burst protocol documented in `docs/protocol.md` of
the ESPARGOS repository, where commands are newline-delimited text and binary
payloads are protected by CRC32:

```
SYNC <nonce>   -> SYNC <nonce>
INFO           -> C6SDR 6 burst 16380
CAPS           -> CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16
VERSION?       -> VERSION {"revision":"unknown",...}
TRANSPORT?     -> TRANSPORT USB 0
RELEASE        -> OK
FREQ <MHz>     -> OK
LIMITS?        -> LIMITS {"gain":[0,0,1],"bandwidth":null,"rates":[...],...}
RANGE?         -> RANGE 2412 2472 1
CAP16 <n> <r>  -> DATA <n> <crc32hex> <us>  +  2*n bytes
CAP20 <n> <r>  -> DATA <n> <crc32hex> <us>  +  ceil(20*n/8) bytes
```

The browser validates each response against strict regular expressions in
`radio.js`:

```js
// radio.js:736
const m = /^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$/.exec(identity);
if (!m || !Number.isInteger(n) || n < 4096 || n > 16384) throw Error('Unsupported SDR firmware: ' + identity);

// radio.js:605
if (!t || (t[1] === 'UART' ? Number(t[2]) !== this.baudRate : Number(t[2]) !== 0))
  throw Error('Invalid serial transport response');

// radio.js:1000
const h = /^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/.exec(header);
```

### 23.2 Three Critical Wire Format Details

**a) Raw word layout.** The C6 dump engine writes one 32-bit word per sample:

```
 31        28 27        20 19        10 9         0
+------------+------------+------------+------------+
|   (used)   |    gain    |     Q      |     I      |
|            |   index    | (10 bits)  | (10 bits)  |
+------------+------------+------------+------------+
```

**I is in bits 9-0 and Q is in bits 19-10**, both signed. The browser unpacks
each 20-bit sample pair with **I in bits 0-9 and Q in bits 10-19**
(`radio.js:98-113`), matching the raw hardware word layout directly, and for
the 8-bit format it expects **I followed by Q** (`radio.js:95-100`).

> **Note on `esp-sdr-zephyr`:** `esp-sdr-zephyr` has them reversed (its
> `esp_sdr_rx_i()` extracts I from bits 19-10 and Q from bits 9-0). The
> authoritative reference is the diagram in `docs/how-it-works.md` of the
> ESPARGOS repository and upstream `main/common/spectrum.c`, which both extract
> I from bits 9-0.

**b) CRC32.** The browser verifies binary payloads using standard zlib `crc32`.
`esdr_crc32()` implements this using a 16-entry nibble lookup table, requiring
only 64 bytes of flash instead of 1,024 bytes for a full 256-entry table.

**c) Sample rate decimation.** The C6 dump engine accepts a clock divider code
in bits 15-16 of the control register: rate codes 0, 1, and 6 produce 80, 40,
and 16 MS/s respectively. `c6_sdr_rx_capture_rate()` configures this divider.

### 23.3 The New Host Modules

```
firmware/main/modules/esp_sdr/
├── esdr_burst_proto.h   210 lines   protocol header: pure C, no ESP-IDF dependencies
├── esdr_burst_proto.c   380 lines   parser, bit packing, CRC32, response builders
├── esdr_host.h           30 lines   host session API
└── esdr_host.c          340 lines   USB Serial/JTAG session handler
```

**`esdr_burst_proto.c`** is written in pure C so the entire wire format can be
verified on the host without hardware. The 24 unit tests in
`test_esdr_burst_proto.c` verify:

| Test | What It Guards |
|---|---|
| `test_the_identity_is_the_one_the_viewer_expects` | `C6SDR 6 burst 16380` |
| `test_the_caps_only_advertise_what_is_implemented` | Ensures unsupported capabilities are not falsely advertised |
| `test_the_limits_are_the_ones_the_viewer_validates` | JSON schema validated by the browser |
| `test_the_transport_is_usb_without_a_baud_rate` | `TRANSPORT USB 0` |
| `test_the_eight_bit_format_is_i_then_q` | I followed by Q, upper 8 bits of each 10-bit sample |
| `test_the_ten_bit_format_decodes_the_way_the_viewer_does` | 20-bit packing and sign extension matching `radio.js` |
| `test_the_crc_is_the_one_the_viewer_computes` | Standard zlib CRC32 test vectors |

**`esdr_host.c`** manages the live USB session. Three requirements must be met:

- **Single port reader.** The `sdr_host` console command executes *inside* the
  console task, so the REPL is paused between reads while the session is active.
  (ESP-IDF's VFS and the USB Serial/JTAG driver share the same ring buffer; see
  `components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c`.)
- **Silent logging.** Because all `ESP_LOGx` output shares the same USB port
  and a single log line in the middle of a binary capture corrupts the frame,
  the **master log level** is set to `ESP_LOG_NONE` during the session (the
  master level overrides per-tag log levels).
- **Exclusive radio ownership.** Because the dump engine uses the Wi-Fi PHY,
  the session acquires a Wi-Fi lease for the duration of the session and
  releases it upon exit (when the browser sends `RELEASE`, the user types
  `MININO`, or the inactivity timeout expires).

Console invocation:

```
minino> sdr_host
Serving the ESP-SDR protocol on the USB port.
Open https://espargos.net/espsdr/app/ and connect to this board.
The console comes back when the viewer sends RELEASE, or type MININO.
```

### 23.4 Coexistence with `osmosdr_serial.c`

`osmosdr_serial.c` remains in the tree for the binary SoapySDR protocol, while
`esdr_burst_proto.h` documents the lineage of the ESPARGOS burst protocol port:

```c
/* Ported from ESPARGOS/esp-sdr (GPL-3.0-or-later): main/common/burst_commands.h,
 * main/common/burst_capture.h, main/common/burst_limits.h and docs/protocol.md.
 * The wire format is fixed by the viewer, so this is a port and not a
 * reimplementation. */
```

### 23.5 Build Fixes Required to Link the SDR Variant

With all stacks enabled (`Bluetooth + Zigbee + Thread`), the DRAM image
measured 301 KB against the 256 KB capture bank cap:

```
ld: SDR: the DRAM image overlaps the capture bank
```

Additionally, clean builds encountered two CMake/Kconfig issues:

| Symptom | Cause |
|---|---|
| `bt_gattc.h:4:10: fatal error: esp_bt.h: No such file or directory` | `minino_kconfig_enabled()` read `sdkconfig`, which does not exist yet on the first CMake pass of a clean build |
| `argtable_ext.h:15:10: fatal error: argtable3/argtable3.h: No such file or directory` | Stale build directory with an outdated component tree |

**Fixing `minino_kconfig_enabled()`.** During a clean build, ESP-IDF generates
`sdkconfig` *after* expanding the component tree. The helper now checks two
sources in order:

1. the build directory's `sdkconfig`, if it already exists;
2. the `SDKCONFIG_DEFAULTS` file list, which is always passed on the CMake
   command line.

And because upstream `CONFIG_BT_ENABLED` is not listed in `sdkconfig.defaults`
by that name, the project added an explicit stack switch:

```cmake
# Kconfig.projbuild
config MININO_BT_STACK
        bool "Enable Bluetooth stack"
        default y
```

```cmake
# across all seven BLE components
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)
```

All three stack switches (`MININO_BT_STACK`, `MININO_ZIGBEE_STACK`,
`MININO_OPENTHREAD_STACK`) are explicitly set in `sdkconfig.defaults` and
profile defaults.

**Fixing menu catalog gates.** `menus_data.c` previously gated Zigbee and
Thread menu entries only on `CONFIG_ZIGBEE_APPS_ENABLE` and
`CONFIG_THREAD_APPS_ENABLE`. When the stacks were disabled in the SDR profile,
linking failed with unresolved symbols:

```
undefined reference to `zigbee_module_switch_enter'
undefined reference to `open_thread_module_sniffer_enter'
undefined reference to `warbee_module_begin'
```

The catalog entries now check both the app toggle and the stack toggle:

```c
#if defined(CONFIG_ZIGBEE_APPS_ENABLE) && defined(CONFIG_MININO_ZIGBEE_STACK)
...
#if defined(CONFIG_THREAD_APPS_ENABLE) && defined(CONFIG_MININO_OPENTHREAD_STACK)
```

**The two build profiles:**

| Variant | Profile | Configuration | Image Size | Flash Margin |
|---|---|---|---|---|
| Standard Boards (Minino, BSides, ...) | `profiles/<board>/profile` | All stacks enabled, **SDR disabled** | 3377 KiB | 8.7% |
| SDR | `profiles/sdr/profile` | Zigbee and Thread disabled, **SDR enabled** | 2496 KiB | 32.5% |

```bash
make builds        # standard board profiles
make build-sdr     # SDR variant
```

The SDR variant reserves the capture bank, enables the linker cap script, and
exposes `Applications > SDR`, `sdr_capture`, and `sdr_host`. Standard board
images do not reserve the capture bank, allowing `.bss` and heap to use the
full SRAM space.

### 23.6 Hardware Findings: Issues Only Visible on a Real ESP32-C6

Testing against an ESP32-C6 (rev v0.2, `48:f6:ee:c2:a2:14`, 8 MB) with a test
harness replicating `radio.js` byte-for-byte uncovered five hardware-level
issues:

**a) Binary payloads larger than 256 bytes were dropped.** The USB Serial/JTAG
driver pushes outgoing data into a 256-byte ring buffer via `xRingbufferSend()`,
which **immediately returns 0 without writing anything if the block exceeds the
ring buffer size**. Passing a 32 KB capture buffer in a single call sent the
`DATA ...` header line and **zero payload bytes**, leaving the browser waiting
forever for the CRC.

```c
/* esdr_host.c: send payload in 64-byte hardware FIFO chunks */
#define ESDR_TX_CHUNK 64
...
  size_t chunk = len - done;
  if (chunk > ESDR_TX_CHUNK) chunk = ESDR_TX_CHUNK;
  int written = usb_serial_jtag_write_bytes(p + done, chunk, pdMS_TO_TICKS(2000));
```

**b) Captures returned constant DC without promiscuous mode and power-save off.**
With the Wi-Fi driver in default STA mode, captures across all 13 channels
returned a constant `I = -63, Q = -62` (zero variance). The dump engine was
running, but the RF front-end was idle/sleeping. Enabling `WIFI_PS_NONE` and
holding promiscuous mode via `radio_lease_prom_hold(RADIO_PROM_SDR_RX)` brought
the receiver chain alive:

| Channel | Before | After |
|---|---|---|
| 2412 MHz | I = -63, var 0 | I = 130, var 9,090, 372 distinct values |
| 2447 MHz | I = -63, var 0 | I = 115, var 1,616 |
| 2472 MHz | I = -63, var 0 | I = 25, var 13,901 |

Dynamic range jumped from a flat line to 87 dB.

**c) The session must hold the radio lease continuously.** Releasing the Wi-Fi
lease after 5 seconds of inactivity and re-acquiring it on the next command
caused the Wi-Fi driver to reinitialize and emit **ROM log lines** that bypass
`esp_log_level_set("*", ESP_LOG_NONE)`, corrupting binary transfers. The
session now acquires the radio once at start and holds it until exit.

**d) Direct `chip_v7_set_chan()` calls conflict with the active Wi-Fi driver.**
While upstream ESPARGOS calls `chip_v7_set_chan()` directly, doing so while the
ESP-IDF Wi-Fi driver is active causes the driver to detect a PHY state mismatch
and hang. Channel tuning is therefore performed through the Wi-Fi driver API.

**e) The entire 128 KiB capture bank must be reserved.** `SRAM_OWNER_REG`
switches ownership of the **entire 128 KiB block 2**, not individual pages.
When only 64 KiB was reserved, the heap placed Wi-Fi driver buffers in the
upper 64 KiB; during shorter captures, CPU access to those buffers stalled and
triggered a task watchdog reset:

```
rst:0x8 (TG1_WDT_HPSYS),boot:0x7c (SPI_FAST_FLASH_BOOT)
```

Reserving all 128 KiB (`0x20000u`) eliminates the stall, and on-board testing
confirmed BLE scanning still works (35 trackers detected, zero
`HCI_ERR_MEM_CAPACITY_EXCEEDED` errors).

### 23.7 On-Chip Spectrum Mode (`SPEC`)

In addition to burst mode, the ESPARGOS viewer supports **On-Chip Spectrum**
mode (`SPEC`), refreshing at dozens of frames per second by computing the FFT
on the ESP32-C6 and streaming power codes.

Implemented in `main/modules/esp_sdr/esdr_spectrum.c`:

| Command | Response |
|---|---|
| `SPECINFO?` | JSON capabilities and profile list |
| `SPEC <ms> <step> <units> <det> <rate> <bins>` | `SPEC <bins> <rate> <bins> <mhz>` followed by binary `SPC1` frames |
| `DC?` / `DC 0` / `DC 1` | `DC <mode>` / `OK` |

Profiles cover all three C6 sample rates (80, 40, and 16 MS/s) across 256, 512,
1024, and 2048 FFT bins.

Three key implementation details:

1. **Task watchdog yielding.** The spectrum loop yields via `vTaskDelay(1)`
   every 20 ms so the idle task can feed the task watchdog.
2. **DIT radix-2 FFT verified against direct DFT.** The host unit test compares
   the Q15 radix-2 FFT output against a reference direct DFT to guarantee bin
   ordering and frequency accuracy.
3. **Per-stage butterfly scaling.** Each butterfly divides by 2 (overall
   transform scale `1/N`) so strong narrowband tones do not saturate Q15
   arithmetic in early stages.

Each frame consists of a 28-byte `SPC1` header, `N` power code bytes in natural
frequency order, and a 4-byte CRC32. Verified on hardware with **28 of 28**
automated spectrum checks passing.

### 23.8 Browser Capabilities Matrix

| Capability | Status |
|---|---|
| Live spectrum and waterfall in browser | Supported (both burst mode and on-chip `SPEC` mode) |
| Tuning from 2412 to 2484 MHz | Supported (2.4 GHz Wi-Fi channels) |
| 80, 40, and 16 MS/s sample rates | Supported via hardware decimation / clock divider |
| 8-bit and 10-bit I/Q sample formats | Supported (`CAP16` and `CAP20`) |
| Manual / Hardware AGC RX Gain (`GAIN`) | Supported (`GAIN 0..60 dB`, 3 dB step) |
| On-chip spectrum (`SPEC`) | Supported (256-2048 bins across all three sample rates; see section 23.7) |
| Analog bandwidth filter (`ALPF`) | Not advertised (fixed per PHY channel) |
| Antenna GPIO switching (`GPIO`) | Not advertised |
| 5 GHz band | Not supported by ESP32-C6 hardware |

### 23.9 Source and Test Files

| File | Description |
|---|---|
| `firmware/main/modules/esp_sdr/esdr_burst_proto.c/.h` | Burst protocol: parser, packing, CRC32, responses |
| `firmware/main/modules/esp_sdr/esdr_host.c/.h` | USB Serial/JTAG host session |
| `firmware/main/modules/esp_sdr/esdr_spectrum.c/.h` | On-chip Q15 FFT, `SPC1` frame builder, and CRC |
| `firmware/main/modules/esp_sdr/esdr_transport.h` | Transport abstraction shared by burst and spectrum modules |
| `firmware/tools/test_esdr_spec.py` | Hardware verification script for `SPEC` mode |
| `firmware/tools/test_esdr_radio.py` | Over-the-air two-board RF verification script |
| `firmware/main/modules/esp_sdr/test/test_esdr_burst_proto.c` | 24 host unit tests for the wire protocol |
| `firmware/main/modules/esp_sdr/c6_sdr_rx.c/.h` | Clock divider (40 and 16 MS/s), overrun canary, channel rounding |
| `firmware/main/modules/cmd_control/cmd_control.c` | `sdr_host` console command |
| `firmware/tools/test_esdr_web.py` | Hardware test script replicating `radio.js` |
| `firmware/sdkconfig.sdr`, `firmware/profiles/sdr/profile` | SDR build variant configuration |
| `firmware/cmake/minino_kconfig.cmake` | CMake Kconfig helper reading both `sdkconfig` and `SDKCONFIG_DEFAULTS` |
| `firmware/components/minino_config/Kconfig.projbuild` | `MININO_BT_STACK` Kconfig symbol |
| `firmware/main/modules/menus_module/menus_data.c` | Menu catalog gates aligned with source filtering |
| `firmware/components/event_bus/event_bus.c/.h` | Fixed `EVENT_SLOT_INVALID` (`0xff`) sentinel |

### 23.10 Usage Guide

```bash
# 0. (Optional) Run automated hardware protocol verification:
python3 tools/test_esdr_web.py /dev/ttyACM0

# 1. Build and flash the SDR variant:
make build-sdr
cd build-sdr && esptool.py write_flash 0x0 minino.bin

# 2. Open a serial terminal on the USB port (e.g. /dev/ttyACM0)
#    and start host mode (or simply open Applications > SDR on the device):
minino> sdr_host

# 3. Open https://espargos.net/espsdr/app/ and click Connect
#    (WebSerial or WebUSB). The browser negotiates SYNC, INFO, CAPS,
#    LIMITS?, FREQ, and starts streaming CAP16/CAP20 or SPEC frames.

# 4. Closing the viewer sends RELEASE and returns control to the console.
```

> **Ignore the "Flash Firmware..." button in the browser**, as it flashes
> upstream ESPARGOS firmware over Minino.

### 23.11 Ergonomics and Future Improvements

- **Automatic vs. manual port handover.** When inside `Applications > SDR` on
  the device, the firmware automatically detects incoming `SYNC` commands from
  the browser and switches into `[HOST]` mode. From the raw CLI prompt outside
  the app, running `sdr_host` pauses the linenoise REPL so characters are not
  echoed back to the browser.
- **Returning to the console.** Closing the browser tab sends `RELEASE`, or
  typing `MININO` from a serial terminal exits host mode immediately.

### 23.12 Over-the-Air Two-Board Validation

> **Added 2026-10-21.**

Tested with two ESP32-C6 boards on the bench (one transmitting, the other
receiving with the SDR). Full details are in section 9 of
[`esp-sdr-integration.md`](./esp-sdr-integration.md) and
[`sdr-resultados.md`](./sdr-resultados.md).

| Test | Result |
|---|---|
| `tools/test_esdr_web.py` on RX board while TX board radiates | 56 of 56 |
| `tools/test_esdr_spec.py` on RX board while TX board radiates | 28 of 28 |
| `make tests` (all 20 host test suites) | 0 failures |
| Native app: burst capture, tuning, and exit | 16,380 words in 218 us |

**Two bugs uncovered and fixed during over-the-air testing:**

1. The Wi-Fi SSID spammer, when launched from the console, **transmitted
   nothing** because the SSID list was only populated when selecting an item in
   the UI menu (`ssid_spam.c:158`). The stored list is now loaded automatically
   on app start (`ssid_spam_load_stored_list()`).
2. Re-entering the SSID spammer **rebooted the board** because
   `esp_netif_create_default_wifi_ap()` asserts if the default AP netif already
   exists (*"duplicate key"*). The netif is now created only if
   `esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")` returns `NULL`.

The `firmware/tools/test_esdr_radio.py` tool measures spectrum differences
across alternating TX-on / TX-off windows, while `firmware/tools/test_esdr_pair.py`
(`app+spec`, `spec+spec`, and `app+app` modes) and `firmware/tools/test_esdr_app.py`
validate both boards simultaneously:

| Board | Burst Protocol | On-Chip Spectrum | Native App |
|---|---|---|---|
| `48:f6:ee:c2:a2:14` | 56/56 | 28/28 | 16/16 |
| `7c:2c:67:42:84:a0` | 56/56 | 28/28 | 16/16 |

When both boards measure the ambient spectrum simultaneously, their spectral
shapes match while showing up to a **14.7 dB absolute level offset** due to
physical antenna placement and multipath (confirmed by simultaneous `wifi_scan`
readings showing the same ambient APs at +13 dB higher RSSI on one board than
the other). And on the receiving board, `wifi_scan` detects the transmitting
board's beacons at -46 dBm, stronger than any ambient access point.

---

## Appendix: Glossary

| Term | Definition |
|---|---|
| **Claim / lease** | Reference-counted ownership request over a shared global hardware or software resource |
| **Fan-out** | Delivering a single event or callback to multiple registered subscribers |
| **Capture bank** | The 128 KiB SRAM block (`0x40840000`) taken over by the C6 hardware dump engine to write I/Q samples |
| **Promiscuous mode** | Wi-Fi driver mode that delivers all received 802.11 frames regardless of destination address |
| **GAP** | Bluetooth *Generic Access Profile*, governing advertising and scanning |
| **GATTC** | Bluetooth *Generic Attribute Profile Client* |
| **BTC task** | Internal Bluedroid task that executes asynchronous Bluetooth stack operations |
| **Host test** | Unit test compiled natively on the workstation with `gcc` against ESP-IDF header mocks |
| **IRAM** | Instruction RAM used by ESP-IDF for time-critical code |
| **`.bss`** | Uninitialized static/global variables that consume SRAM at runtime |
| **Priority preemption / inversion** | A higher-priority task preempting or waiting on a lower-priority task |
| **`-zmuldefs`** | Linker flag permitting multiple symbol definitions (previously masking header-defined data arrays) |

## Appendix: References

| Document | Contents |
|---|---|
| [`docs/esp-sdr-integration.md`](./esp-sdr-integration.md) | Detailed SDR technical reference: registers, capture bank, SRAM budget |
| [`docs/rf-spectrum-advanced.md`](./rf-spectrum-advanced.md) | RF Spectrum Analyzer documentation |
| [`firmware/README.md`](../firmware/README.md) | Firmware build instructions, board profiles, and build variants |
| [`firmware/menus.md`](../firmware/menus.md) | Menu tree reference |
| [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr) | Upstream ESP-SDR reference project |
| [esp-sdr-zephyr](../esp-sdr-zephyr/) | Upstream Zephyr port used for capture bank linker comparison |
