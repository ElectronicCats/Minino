# Surveillance Detector for Minino - Design

Date: 2026-08-23
Status: Approved for implementation plan

## 1. Objective

Equip Minino with a passive surveillance infrastructure detector: Flock Safety cameras and license plate readers (ALPR), body cams, personal trackers, IP cameras, drones, and skimmers. Alerting is local (OLED, buzzer, LEDs) and each detection is logged with GPS coordinates to microSD for subsequent mapping.

This work synthesizes two existing projects:

- [flock-you](https://github.com/colonelpanichacks/flock-you) (MIT) - the Flock detection engine with confidence levels and Information Element fingerprinting.
- [eye-spy](https://github.com/simeononsecurity/eye-spy) (Apache-2.0) - the broad scope of detection engines and the cumulative score model.

This is not a port: Minino runs on ESP32-C6 with ESP-IDF in C, whereas both reference projects use Arduino/PlatformIO on ESP32-S3 and ESP32-PICO-D4. We reimplement the algorithm, not the code.

## 2. Decisions Made

| Decision | Value |
|---|---|
| Detection scope | Broad surveillance (eye-spy model), embedding the Flock engine from flock-you |
| Output | Local alert (OLED + buzzer + LEDs) and GPS log to microSD. No USB streaming or web dashboard |
| OPSEC | Passive by default; active WiFi scan as an option in settings |
| Signatures | Base table compiled in flash + optional overlay from microSD |
| Overlap with `trackers_scanner` | Extract the BLE dissector to the shared component; no duplication |
| Radio testing | Two Minino boards (transmitter + detector). Do not rely on self-loopback |

## 3. Scope

### In Scope

- `surveillance_detect` component with BLE and WiFi engines.
- Menu app with screens, buzzer, and LEDs.
- CSV logging with GPS to microSD, compatible with WiGLE uploads.
- GPX export of tier 4 detections with coordinates.
- PCAP dump of tier 3 and 4 evidence.
- Signature overlay from microSD.
- Refactor of `trackers_scanner` to use the shared dissector.
- Host unit tests for matchers and the scoring engine.

### Out of Scope

- Flask dashboard or any host tool.
- JSON streaming over USB.
- 5 GHz detection (the ESP32-C6 is 2.4 GHz only).
- Automated contribution to DeFlock (it is OpenStreetMap; GPX is a helper, not an integration).

## 4. Architecture

### 4.1 Component `firmware/components/surveillance_detect/`

Prefix `surv_` to avoid colliding with `sd_card`. No dependencies on OLED, menus, or screens.

```
surveillance_detect/
├── CMakeLists.txt
├── include/
│   ├── surv_types.h            surv_event_t, surv_class_t, surv_proto_t
│   ├── surv_match.h            pure matchers
│   ├── surv_engine.h           score, decay, cooldown, per-MAC table
│   └── surveillance_detect.h   public API
├── surv_signatures.c           base tables in flash
├── surv_overlay.c              microSD overlay parser
├── surv_match.c                matchers (hardware-free, host-testable)
├── surv_engine.c               scoring and tier engine
├── surv_radio.c                BLE/WiFi phase scheduler
└── test_apps/                  Unity on host
```

`surv_match.c` includes no ESP-IDF headers beyond basic types: this is required for tests to run on the host.

### 4.2 App `firmware/main/apps/surveillance/`

- `surveillance_module.c/.h` - lifecycle, button callbacks, CSV/GPX/pcap writing.
- `surveillance_screens.c/.h` - OLED screens, following the pattern of `main/apps/ble/trackers/trackers_screens.c`.

New menu entry under `MENU_APPLICATIONS` (not under `MENU_WIFI_APPS`, because it uses both radios), declared in `main/modules/menus_module/menus_include/menus.h` and gated by Kconfig like the rest of the apps.

### 4.3 Data Flow

```
WiFi promiscuous callback ─┐
                           ├─→ fast matcher ─→ ring buffer (32) ─→ task surv_engine
BLE GAP callback          ─┘   no printf/malloc                          │
                                                                         ▼
                                             score + per-MAC dedupe ──→ app callback
                                                                         │
                                                             ┌───────────┼───────────┐
                                                           OLED       buzzer    CSV/GPX/pcap
```

The promiscuous callback runs in the WiFi task with real-time constraints: it only compares and enqueues. Writing to microSD never happens in the engine task (an `sd_card_append_to_file` call taking tens of ms would stall ring buffer draining and drop frames); instead, data is buffered in RAM and flushed in batches, matching `main/modules/gps/wardriving/wardriving_module.c`.

### 4.4 Tasks

| Task | Priority | Stack | Responsibility |
|---|---|---|---|
| `surv_radio` | 10 | 3 KB | BLE/WiFi phases and channel hopping |
| `surv_engine` | 5 | 4 KB | Drains ring buffer, score, dedupe, callbacks |
| (WiFi task) | - | - | Promiscuous callback: compares and enqueues |
| (App task) | - | - | UI and batch flushing to microSD |

## 5. Detection Model

The two reference projects use distinct models that answer distinct questions. Both are implemented.

- **Tier (0-4)**, from flock-you - *"how confident am I about this device?"*. Per-MAC property. A high tier is never overwritten by a lower one and can bypass the dedupe cooldown: without that exception, a tier 1 hit (which triggers on any frame) wins the race to the 5 s cooldown and hides the tier 4 confirmation from the same device. It determines the buzzer tone and the saved label.
- **Score**, from eye-spy - *"how surveilled is this location?"*. Global accumulator: adds points per class, decays -1 every 60 s, with a 120 s per-class cooldown so a persistent device cannot accumulate without bound. It determines the status level (0-2 CLEAR, 3-5 CAUTION, 6+ ALERT).

### 5.1 Event

```c
typedef struct {
  uint8_t      mac[6];
  surv_class_t klass;    // FLOCK, ALPR, AXON, AIRTAG, CAM, ODID, SKIMMER, ...
  uint8_t      tier;     // 0-4, confidence of the triggering method
  int8_t       rssi;
  uint8_t      channel;  // 0 if BLE
  surv_proto_t proto;    // BLE | WIFI
} surv_event_t;
```

The engine maintains a per-MAC table (cap 200, like flock-you) with `best_tier` and `methods_seen`, plus the global accumulator with its decay.

`tier` and `points` are independent axes and must not be derived from one another. An SSID containing "flock" is weak evidence that *that specific* device is a camera (an SSID is trivial to spoof), but it is strong evidence that the area has Flock infrastructure. That is why it can be tier 0 while simultaneously adding 5 points to the score. SSID keyword matching is **enabled** by default (flock-you ships with it disabled), specifically because at tier 0 it never labels a device as confirmed.

### 5.2 WiFi Tiers (from flock-you)

| Tier | Method | Condition |
|---|---|---|
| 4 | `wildcard_probe_ie_sig` | Probe Request + length-0 SSID IE + OUI in `addr2` + IE signature |
| 3 | `wildcard_probe` | Same, without IE signature match |
| 2 | `oui_addr2` | OUI in `addr2` of any frame |
| 1 | `oui_addr1` / `oui_addr3` | OUI in `addr1` or BSSID; prone to false positives |
| 0 | `ssid_kw` | Keyword in SSID |

Tiers 1 and 2 are echoes: a nearby AP responding to a camera's probe places the camera's MAC in `addr1`. They are kept because they cover stations that do not transmit during the window, but at degraded confidence.

### 5.3 Engines

| Source | Engines |
|---|---|
| flock-you | The five tiers in the table above |
| eye-spy | Axon (OUI `00:25:df`), Ray-Ban Meta (UUID `0xFD5F`), Flock BLE by name, Flock BLE by mfr ID `0x09C8`, Raven (GATT UUIDs), HC-03/05/06 skimmers, ODID BLE (`0xFFFA`), ODID WiFi (NaN to `51:6f:9a:01:00:00`), iBeacon, MeshCore, SoundThinking/ShotSpotter, camera and ALPR OUI and SSID, unknown MAC persistence |
| `trackers_scanner` (already in Minino) | AirTag, SmartTag, Tile, Apple Nearby |
| New in Minino | Promiscuous beacons -> BSSID and SSID **without transmitting** |

The last item is an improvement over both: eye-spy requires `esp_wifi_scan` (which emits probe requests and gives away the device) to obtain BSSID/SSID; sniffing beacons in promiscuous mode obtains the same data without transmitting. flock-you does not inspect beacons.

`SOUNDTHINKING_OUIS`, `RAVEN_UUIDS`, and mfr ID `0x09C8` are present in the eye-spy codebase but not in its README; they are included.

## 6. Signatures

### 6.1 Base Table

A single table storing class and weight as data, instead of eye-spy's five separate arrays (`FLOCK_OUIS` 35, `FLOCK_MFR_OUIS` 6, `SOUNDTHINKING_OUIS` 1, `ALPR_OUIS` 1, `CAM_OUIS` 31 = 74 OUIs):

```c
typedef struct {
  uint8_t      oui[3];
  surv_class_t klass;
  uint8_t      points;   // weight in global score
  uint8_t      tier;     // confidence ceiling for this signature
} surv_oui_entry_t;
```

The event tier is determined by the **method** that triggered (address position, wildcard probe, IE signature), not the table. The entry's `tier` field is a **ceiling**: contract manufacturer OUIs (Liteon, USI), which appear in non-Flock hardware, never exceed tier 1 even if the method would yield a higher tier.

74 entries x 5 B = 370 bytes. Sorted, with a **32-byte bitmap prefilter indexed by the first octet**: if the bit is not set, the frame is discarded in a single operation. This matters because the matcher runs inside the promiscuous callback on every received frame.

Do not add a "skip locally administered MAC" filter: `82:6b:f2` has that bit set and is a field-confirmed Flock camera reported by DeFlockJoplin.

### 6.2 IE Signature, in Binary

flock-you builds the signature as a string and compares it with `strcmp` inside the promiscuous callback, violating its own rule against formatting in the hot path. The drive-tested signature is:

```
2,12,127,221:506f9a16030103,45,191,221:0050f208000000
```

Here it is stored precompiled as tokens and compared by walking the frame's IEs and the pattern in parallel:

```c
typedef struct { uint8_t tag; uint8_t vlen; uint8_t vendor[7]; } surv_ie_tok_t;
// {2} {12} {127} {221,7,50 6f 9a 16 03 01 03} {45} {191} {221,7,00 50 f2 08 00 00 00}
```

Same result, without string formatting in the callback.

The field patches from flock-you are preserved as-is, because they represent real field findings rather than noise. **This is not optional:** the first implementation omitted them and, when measured against the original algorithm across 25,000 inputs, failed 3,020 times - all in the direction of failing to recognize cameras that the reference implementation recognizes, without any conventional unit test catching it. Equivalence is now verified by a permanent differential test that runs both algorithms in parallel.

- *Phantom overflow*: TLVs with impossible lengths; up to 16 are skipped per frame.
- TLV resync when traversal loses alignment.
- Retry with the frame trimmed by 4 bytes (FCS).
- Canonicalization anchored to the LiteON vendor prefix `221:506f9a16030103`.

### 6.3 Overlay from microSD

File `/surveil/signatures.csv`, plain text. `+` adds, `-` removes:

```
+oui,70:c9:4e,flock,5,2
-oui,f8:a2:d6
+ssid,alpr,alpr,4,0
+blename,pigvision,flock,5,0
+uuid,fd5f,glasses,5,0
+iesig,2,12,127,221:506f9a16030103,45,191,221:0050f208000000
```

Loaded once in `surv_begin()`, never from a callback. Malformed lines are ignored and counted. Limits: 256 OUIs, 64 keywords, 8 IE signatures. Without a microSD card or file, it operates with the base table. The startup screen displays `sigs: 74 base +12 SD` or `SD: none`.

Using plain text rather than binary allows uploading an updated list over WiFi via the `web_file_browser` that Minino already includes, without removing the card or recompiling. Neither reference project can do this.

Making the IE signature updatable via overlay is not a luxury: it is what will change when Flock updates its camera firmware over OTA.

## 7. Radio Scheduler

The ESP32-C6 has a single 2.4 GHz antenna shared across WiFi, BLE, and 802.15.4. With broad detection scope, everything cannot be covered simultaneously, so airtime allocation is an explicit user choice in settings:

| Profile | Allocation | Use Case |
|---|---|---|
| `Flock/ALPR` | 100% promiscuous WiFi | Car wardriving; maximum probability of catching probes |
| `Surveillance` | 20 s cycle: passive BLE 6 s -> promiscuous WiFi 14 s | General use |
| `Trackers` | 100% passive BLE | On foot: body cams, AirTags, smart glasses |

Cameras emit wildcard probes every ~125 ms while hopping channels in ascending order, so every second spent not listening is a potentially missed camera. The mixed profile gives up roughly 30% of WiFi airtime.

**Window transitions.** Both stacks remain initialized; only scanning is mutually exclusive: `esp_ble_gap_stop_scanning()` before `esp_wifi_set_promiscuous(true)` and vice versa. Do not call `esp_bluedroid_deinit`/`esp_bt_controller_deinit` every cycle like eye-spy does: it fragments the heap and costs hundreds of ms every 20 s.

**Passive BLE**: custom parameters using `BLE_SCAN_TYPE_PASSIVE` and `window == interval` (continuous listening). Do not use the `bt_gattc` default, which is currently `BLE_SCAN_TYPE_ACTIVE` (`components/bt_gattc/bt_gattc.c:61`).

**Channel hopping**: descending 11/6/1 with 250 ms dwell, matching flock-you, calibrated against the camera's ascending hop order. In the `Surveillance` profile, every fourth pass extends to 13/8/3 to cover ODID, matching eye-spy.

**Region clamping**: the channel set is clamped to the configured region's `nchan`. Minino resolves regions in `main/modules/settings/wifi/wifi_regions.c` and defaults to `GLOBAL` = channels 1-11; hopping to channel 13 there hears nothing. eye-spy has this bug (it hops to 13 without checking region).

**Optional active scan**: when enabled by the user in settings, a 3 s `esp_wifi_scan` window is appended at the end of the cycle. Disabled by default. Only available in the `Flock/ALPR` and `Surveillance` profiles; ignored in `Trackers`. Enabling it causes the device to transmit probe requests and cease being undetectable: the screen indicates this with a `!` next to the profile.

**Integration with `radio_selector`**: currently it consists of two flags (`platform_configured`, `stack_initialized`) only checked by Zigbee and Thread. `RADIO_SELECT_SURVEILLANCE` is added to the enum, and upon entering the app, if `radio_selector_is_stack_initialized()` is true, a warning is shown and the app does not start, avoiding antenna contention with 802.15.4.

**Clean shutdown** on exit: promiscuous off, `esp_wifi_stop()`, BLE scan stopped, tasks deleted, and final CSV buffer flush. This follows the `deauth_detector_stop()` pattern.

## 8. Persistence and Export

### 8.1 CSV

Reuses the provenance header from `wardriving_module` (`ElecCats-1.0`, WigleWifi format) and appends custom columns at the end:

```
MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,
CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type,
Class,Tier,Method,Score
```

Running `cut -d, -f1-14` yields a valid WiGLE file for upload; the full file preserves class, tier, and method. Stored in the `surveil/` directory alongside `warfi/`, `warbee/`, and `thread/`. Up to 200 bytes per line (the 14 WigleWifi columns plus our four custom columns do not fit in the 150 B `CSV_LINE_SIZE` used by wardriving), 200-line buffer, flushed in batches.

`Type` takes the values `WIFI` or `BLE`, matching WiGLE.

**Without a GPS fix, the detection is still saved**, with empty latitude and longitude fields. Losing a confirmed camera because GPS has not acquired a fix yet would be the worst possible tradeoff; filtering out rows without coordinates before uploading is trivial.

### 8.2 GPX

File `surveil/flock_<date>.gpx` containing one waypoint per tier 4 detection that has coordinates. Loaded as a layer in the OpenStreetMap iD editor to place nodes in DeFlock.

DeFlock does **not** consume CSV: it is OpenStreetMap, with tagged nodes (`man_made=surveillance`, type, orientation) created from its app or from iD. The GPX file is an aid to avoid repeating the drive, not an integration.

### 8.3 PCAP Evidence

For tiers 3 and 4, the raw frame is dumped to `surveil/evidence_<date>.pcap` using `managed_components/espressif__pcap`, which is already in the project and used by `wifi_sniffer`. Copying only happens after a match has already triggered, so it adds no weight to the hot path. The file is capped at 4 MB per session; once reached, frame writing stops and an on-screen notice is shown without stopping detection or CSV logging.

This makes it possible to open the capture in Wireshark and verify the IE signature manually instead of trusting the firmware blindly. Neither reference project saves raw evidence. These same dumps serve as test vectors for host unit tests.

## 9. UX

### 9.1 Main Screen (SH1106 128x64)

```
SURVEIL  [FLOCK]
████████░░ ALERT
Flock  x3   T4
-52dBm  ch 6
GPS 12sat  SD ok
sigs 74+12
```

Plus a button-navigable detection list, following the `trackers_screens` pattern.

### 9.2 Per-Tier Buzzer

Taken from flock-you, to distinguish the detection method by ear while driving:

| Tier | Tone |
|---|---|
| 4 | Ascending chirp 2000 -> 2800 Hz |
| 3 | Ascending chirp 1400 -> 1800 Hz |
| 2 | Blip 1200 Hz |
| 1 | Blip 800 Hz |
| 0 | Blip 600 Hz |

Per-tier mute mask in settings, persisted via `preferences_put_uchar("surv_beep", mask)`. Muting only affects the buzzer: detections are still logged.

### 9.3 LEDs

eye-spy renders the status level on an RGB NeoPixel. Minino does not have RGB LEDs: `components/leds/include/leds.h` exposes `LED_LEFT` and `LED_RIGHT` with brightness only, no color. The status level is shown on screen and the LEDs encode the level via cadence:

| Level | LEDs |
|---|---|
| CLEAR (0-2) | Off |
| CAUTION (3-5) | Slow `led_start_breath` |
| ALERT (6+) | Fast `led_start_blink` |

### 9.4 Regional Notice

Flock Safety is deployed in the United States and, to a lesser extent, Canada. With the base table in Mexico, the app can run for an hour without a single detection, and that is the expected result, not a bug. The app's help screen states this explicitly and points to the microSD overlay for loading local signatures.

## 10. Changes to Existing Code

| File | Change |
|---|---|
| `components/trackers_scanner/trackers_scanner.c` | Move the BLE advertising dissector to `surv_match.c`; the component calls it while keeping its list and UI. Switches to passive scan |
| `components/bt_gattc/bt_gattc.c` | Allow passive scan parameters without changing the default for other consumers |
| `components/radio_selector/` | Add `RADIO_SELECT_SURVEILLANCE` and its setter |
| `main/modules/menus_module/menus_include/menus.h` | New menu entry and `menu_idx` |
| `main/modules/settings/` | Radio profile, active scan toggle, buzzer mask |

Refactoring `trackers_scanner` (219 lines plus its app) carries regression risk and must be accompanied by manual verification of the trackers app before proceeding.

## 11. Testing and Acceptance Criteria

### 11.1 Host Tests (Unity)

Using the `test_apps` pattern already used by `components/console`, hooked into existing CI (`.github/workflows/builds.yml`):

- Every OUI in the base table matches its class.
- Mutual exclusion: no OUI appears in two classes.
- Case-insensitive SSID keywords; BLE names by substring.
- Overlay parser: valid line, malformed line, `-` removal, entry limit, missing file, empty file.
- IE fingerprint against real frame vectors, including phantom overflow, TLV resync, and frames with and without FCS.
- Engine: -1/60 s decay, 120 s per-class cooldown, high tier never degrades, high tier bypasses dedupe.

Frame vectors come from the PCAP dumps described in section 8.3.

### 11.2 Two-Board Radio Tests

The parser is tested on the host by injecting buffers directly. Testing the radio, channels, and timing requires **two Minino boards**: one transmitting synthetic frames via `esp_wifi_80211_tx` (already used in `ssid_spam` and `drone_id/spoofer`) and the other detecting. Do not rely on a single board receiving its own transmissions in promiscuous mode: the radio is half-duplex and self-reception is not supported.

### 11.3 Validation Without Nearby Flock Cameras

The overlay solves this: add the OUI of a personal device to `/surveil/signatures.csv` and verify that it triggers the expected tier and outputs a well-formed CSV line. Tier 4 is validated by replaying a dataset frame from the second board. Without this path, "detects nothing" is indistinguishable from "broken".

### 11.4 Acceptance Criteria

| Criterion | Threshold |
|---|---|
| False positives with base table, 30 min in an urban area in Mexico | 0 |
| Detection of synthetic tier-4 frame with active channel hopping | < 2 s |
| Ring buffer overflow in a 1 h session | 0 (counter visible on debug screen) |
| Ring buffer drain stall caused by microSD writes | < 50 ms |
| Trackers app after refactor | No regression, manually verified |

## 11.5 Implementation Order

The scope is large and should be executed in this order, with each stage independently verifiable:

1. `surv_match` + `surv_signatures` + host tests. No radio, no UI.
2. Refactor `trackers_scanner` onto the shared dissector, with manual verification of the trackers app. Regression risk isolated and resolved early.
3. `surv_engine` (score, tiers, dedupe) + host tests.
4. `surv_radio` and the WiFi/BLE engines on hardware, `Flock/ALPR` profile first.
5. App, screens, buzzer, and LEDs.
6. Persistence: CSV, then GPX, then PCAP.
7. microSD overlay.
8. Two-board radio tests and acceptance criteria validation.

## 12. Licenses and Credits

Minino is GPL-3.0. MIT (flock-you) and Apache-2.0 (eye-spy) are compatible with GPLv3 when preserving copyright notices. Credits are preserved in the headers of files derived from each research source and on the About screen:

- **OrdoOuroboros / @NitekryDPaul** - Flock OUI list and `addr1` detection technique.
- **DeFlockJoplin** - wildcard probe request signature, IE fingerprint, and OUI `82:6b:f2`.
- **Lucia Pintor and Luigi Atzori** - *Analysis of Wi-Fi Probe Requests Towards Information Element Fingerprinting*, GLOBECOM 2022, doi:10.1109/GLOBECOM48099.2022.10001618.
- **colonelpanichacks** - flock-you.
- **SimeonOnSecurity** - eye-spy.
- **DeFlock (FoggedLens)** - ALPR location data and methodology.

## 13. Risks

| Risk | Mitigation |
|---|---|
| Refactoring `trackers_scanner` breaks a currently working app | Manual verification before proceeding; refactor goes first and in isolation |
| MAC randomization blinds tiers 1-2 | Documented; tier 4 does not rely on OUI alone |
| IE signature stops matching after a Flock OTA update | Signature is updatable via microSD overlay |
| Radio contention with 802.15.4 | Check `radio_selector_is_stack_initialized()` on entry |
| microSD wear | Batch flushing every 200 lines, matching wardriving |
| Expectation of detections in Mexico | Regional notice on the help screen |

## 14. Out of Scope for This Design, Possible Later

- JSON streaming over USB for a host dashboard.
- Flock detection over BLE (stopped working in spring 2026 according to flock-you; re-evaluate if it returns).
- Direct import of WiGLE web export format as an overlay.
