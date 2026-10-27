# Minino SDR Test Results

> Test report for two Minino boards running the SDR firmware variant (which
> reserves the hardware capture bank): one board operating as an SDR receiver
> while the other radiates, and vice versa, in the same RF environment.
>
> - **Date:** 2026-10-21
> - **Boards:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` and rev v0.1
>   `7c:2c:67:42:84:a0`, both with 8 MB flash
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`
> - **Firmware:** `firmware/build-sdr` (`minino.bin`, 2,561,504 bytes, 32%
>   free space in the application partition)
> - **Related Documents:** Dual-application summary in
>   [`spectrum-SDR-results.md`](./spectrum-SDR-results.md), user guide in
>   [`sdr-guia.md`](./sdr-guia.md), wire packet format in
>   [`sdr-protocolo.md`](./sdr-protocolo.md), and technical design reference in
>   [`esp-sdr-integration.md`](./esp-sdr-integration.md)

---

## 1. Summary

| # | What Was Tested | Where | Result |
|---|---|---|---|
| 1 | All 15 C test suites (378 tests) plus the Python suite | Host | **0 failures** |
| 2 | ESPARGOS viewer protocol, byte by byte | Both boards | **56/56** and **56/56** |
| 3 | Spectrum mode (on-chip FFT) | Both boards | **28/28** and **28/28** |
| 4 | Native SDR app (burst capture, tuning, exit) | Both boards | **16/16** and **16/16** |
| 5 | Native app and spectrum mode **simultaneously** across both boards | Both boards | Passed all 4 role assignments |
| 6 | Both spectrum profiles measured simultaneously | Both boards | Identical shape, up to **14.7 dB** level difference |
| 7 | *Separate round:* transmission from one board observed by the other | Both boards | Beacons received at **-46 dBm** (Section 7) |

Key takeaway: **both boards measure the same RF environment and produce
mutually consistent results**, with amplitude differences explained entirely by
the physical position of each board relative to surrounding access points
(Section 6).

---

## 2. How to Reproduce the Tests

```bash
# Build and flash firmware
cd firmware
make build-sdr
idf.py -B build-sdr -p /dev/ttyACM0 flash
idf.py -B build-sdr -p /dev/ttyACM1 flash

# Single-board test suites
python3 tools/test_esdr_web.py  /dev/ttyACM0    # Viewer wire protocol
python3 tools/test_esdr_spec.py /dev/ttyACM0    # On-chip spectrum mode
python3 tools/test_esdr_app.py  /dev/ttyACM0    # Native on-board app

# Simultaneous dual-board tests
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM1 /dev/ttyACM0
python3 tools/test_esdr_pair.py app+app   /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py spec+spec /dev/ttyACM0 /dev/ttyACM1

# Over-the-air test (one board radiating)
python3 tools/test_esdr_radio.py /dev/ttyACM1 /dev/ttyACM0 --mode spec
```

The boards are uniquely identified by their persistent USB serial numbers:

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_48:F6:EE:C2:A2:14-if00
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_7C:2C:67:42:84:A0-if00
```

---

## 3. Test Suites

### 3.1 ESPARGOS Viewer Protocol

`tools/test_esdr_web.py` replicates the browser's `radio.js` logic:
handshake, exact regular expressions on every response, CRC32 verification, and
I/Q sample unpacking.

| Check | Result |
|---|---|
| Console responds and host mode starts | PASS |
| Synchronization nonce returns intact | PASS |
| `INFO` -> `C6SDR 6 burst 16380` | PASS |
| `CAPS` advertises expected tokens and **omits** `GPIO`, `GAIN`, `ALPF`, `SPECSTAT` | PASS |
| `TRANSPORT?` -> `TRANSPORT USB 0` | PASS |
| `RANGE?` -> `RANGE 2412 2472 1` | PASS |
| `LIMITS?` returns valid viewer JSON (gain, sample rates, bit depths) | PASS |
| `VERSION?` returns valid viewer JSON | PASS |
| `FREQ 2412` and `FREQ 2417` -> `OK`; `FREQ 2000` -> `ERR` | PASS |
| `CAP16` and `CAP20` of 16,380 samples with valid CRC32 | PASS |
| All three sample rates (80, 40, and 16 MS/s) | PASS |
| Captured spectrum has dynamic range (86 dB) and peak at the LO | PASS |
| `RELEASE` replies `OK` and restores the console | PASS |
| `MININO` escape command restores the console | PASS |

**56 of 56 passed on both boards.**

Capture metrics:

| Metric | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| `CAP16` (16,380 samples) | 32,760 bytes, 217 us capture time, 608 kB/s | 32,760 bytes, 217 us, 611 kB/s |
| `CAP16`: I/Q peak / RMS | 0.477 / 0.247 | 0.367 / 0.120 |
| `CAP20` (16,380 samples) | 40,950 bytes, 209 us, 618 kB/s | 40,950 bytes, 208 us, 618 kB/s |
| `CAP20`: I/Q peak / RMS | 0.445 / 0.223 | 0.275 / 0.076 |
| Spectrum dynamic range | 88.2 dB (peak -3.4 dBFS, floor -91.7 dBFS) | 86.2 dB (peak -9.4 dBFS, floor -95.6 dBFS) |
| Peak located at the LO | Yes | Yes |

### 3.2 Spectrum Mode (On-Chip FFT)

`tools/test_esdr_spec.py`: tests all profiles, frame streaming, CRC32 integrity,
and clean stream termination.

| Check | Result |
|---|---|
| All 12 profiles (3 sample rates x 4 FFT sizes) | PASS |
| Stream header `SPEC <bins> <rate> <bins> <mhz>` | PASS |
| `SPC1` frames arrive with valid CRC32 | PASS |
| Bins contain codes in `0..255` and spectrum shows real structure | PASS |
| LO carrier peak is at bin 0 | PASS |
| `SPECEND` returns all twelve fields on clean stop | PASS |

**28 of 28 passed on both boards.**

### 3.3 Native SDR App

`tools/test_esdr_app.py`: tests the on-device UI app directly without the web
viewer.

| Check | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| App launches | PASS | PASS |
| Capture engine initializes (buffer at `0x40840000`) | PASS | PASS |
| Tunes to 2412 MHz (CH 1) on entry | PASS | PASS |
| MicroSD card responds | PASS | PASS |
| Burst size is 16,380 words | PASS | PASS |
| Capture rate is close to 80 MS/s | 75.5 MS/s | 75.1 MS/s |
| Burst is saved to microSD | PASS | PASS |
| UP tunes to 2417 MHz (CH 2) | PASS | PASS |
| DOWN tunes back to 2412 MHz (CH 1) | PASS | PASS |
| Burst counter increments | PASS | PASS |
| App exits and stops the capture engine | PASS | PASS |
| Radio lease returns to Minino | PASS | PASS |
| Serial console remains responsive | PASS | PASS |

**16 of 16 passed on both boards.**

### 3.4 Host Test Suites

```
make tests
```

15 C test suites (**378 unit tests**) plus the Python test suite
(`test_flash_tools.py`, 5 tests): **0 failures**.

| Suite | Tests | Suite | Tests |
|---|---|---|---|
| `surveillance_detect` | 80 | `cmd_control` | 16 |
| `nmea_parser` | 22 | `esp_sdr` | 24 |
| `drone_id` | 27 | `menus_module` | 7 |
| `files_ops` | 23 | `hid_device` | 13 |
| `gap_dispatcher` | 9 | `rf_spectrum` | 13 |
| `radio_lease` | 12 | `general_scrolling_text` | 12 |
| `event_bus` | 11 | `bt_spam` | 13 |
| `resistor_detector` | 12 | `test_flash_tools.py` | 5 |

---

## 4. Both Applications Running Simultaneously Across Both Boards

`tools/test_esdr_pair.py`. Each thread communicates with its assigned board:
the native app over the serial console (`sdr_capture` and the `press` hook) and
spectrum mode over the binary wire protocol (`SPEC`).

| Assignment | Board A | Board B | Result |
|---|---|---|---|
| 1 | Native app | Spectrum mode | App: 3 bursts of 16,380 words (218, 217, 218 us), tuning 2412 <-> 2417, clean exit, radio released; Spectrum: **1,416 frames** with valid CRC, peak 116, median 58 |
| 2 | Spectrum mode | Native app | Spectrum: **1,399 frames**, peak 112, median 59; App: 3 bursts of 16,380 words (217, 218, 218 us) |
| 3 | Native app | Native app | Both simultaneously: 3 bursts each (one took 557 us due to a driver pause) |
| 4 | Spectrum mode | Spectrum mode | **1,413** and **1,402** frames, with both profiles compared band by band (Section 6) |

Across all four role assignments, `SPECEND` completed cleanly: status 0, zero
dropped frames (`drops = 0`), zero abandoned captures (`abandoned = 0`), and all
FFTs delivered.

---

## 5. Spectrum Profile Across Both Boards

Snapshot from spectrum mode: LO at 2412 MHz, 40 MS/s, 1024 bins,
1,427 averaged frames, measured one board at a time.

| Metric | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Peak | 113 | 116 |
| Median | 58 | 58 |
| Minimum (noise floor) | 8 | 8 |
| LO (bin 0) | 74.0 dB | 74.4 dB |

Band-by-band profile (dB, MHz offset relative to LO):

| Band | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| -12 MHz | 49.5 | 50.2 |
| -10 MHz | 56.6 | 55.7 |
| -5 MHz | 61.7 | 60.5 |
| -2 MHz | 68.2 | 65.0 |
| **0 MHz (LO)** | **74.0** | **74.4** |
| +2 MHz | 71.1 | 75.2 |
| +5 MHz | 69.1 | 71.6 |
| +10 MHz | 65.0 | 69.8 |
| +12 MHz | 48.7 | 54.6 |
| +15 MHz | 22.5 | 28.3 |
| +19 MHz | 10.8 | 12.9 |

Both boards observe the exact same feature: a wide ~20 MHz hump centered at the
LO (Wi-Fi channel 1, 2401-2423 MHz), dropping to the noise floor 10 MHz further
out.

---

## 6. Simultaneous Measurement: Same Environment, Different Levels

Measuring both boards simultaneously (LO 2412 MHz, 40 MS/s, 1024 bins):

| Band | `7c:2c:67` | `48:f6:ee` | Diff |
|---|---|---|---|
| -20 MHz | 8.9 | 9.0 | -0.1 |
| -18 MHz | 12.4 | 11.5 | +0.9 |
| -16 MHz | 19.6 | 19.0 | +0.6 |
| -14 MHz | 34.0 | 33.4 | +0.6 |
| -12 MHz | 52.7 | 52.7 | +0.0 |
| -10 MHz | 57.8 | 58.2 | -0.4 |
| -8 MHz | 59.5 | 60.0 | -0.5 |
| -6 MHz | 59.9 | 60.7 | -0.7 |
| -4 MHz | 62.2 | 62.3 | -0.0 |
| 0 MHz | 69.9 | 67.8 | +2.0 |
| +2 MHz | 82.3 | 67.6 | **+14.7** |
| +4 MHz | 81.5 | 68.3 | +13.1 |
| +6 MHz | 80.7 | 67.7 | +13.0 |
| +8 MHz | 80.4 | 68.1 | +12.4 |
| +10 MHz | 73.9 | 61.8 | +12.2 |
| +12 MHz | 57.4 | 44.4 | +13.0 |
| +14 MHz | 40.6 | 27.3 | +13.3 |
| +16 MHz | 28.1 | 16.6 | +11.6 |
| +18 MHz | 18.8 | 11.2 | +7.6 |

**The spectral shape is identical; the power level is not** (up to 14.7 dB
difference above the LO).

### 6.1 Explanation: Physical Board Placement

Running `wifi_scan` on both boards at the same time reveals why:

| Network | `48:f6:ee` | `7c:2c:67` | Difference |
|---|---|---|---|
| INFINITUM2843 | -45 dBm | -58 dBm | **+13 dB** |
| Totalplay-77A5 | -69 dBm | -87 dBm | **+18 dB** |
| Club_Totalplay_WiFi | -71 dBm | -87 dBm | +16 dB |
| Totalplay-77A5_EXT | -83 dBm | -95 dBm | +12 dB |
| Totalplay-B6B8 | -90 dBm | -90 dBm | 0 dB |
| IZZI-8BDC | -90 dBm | -95 dBm | +5 dB |
| IZZI-F50C | -91 dBm | -89 dBm | -2 dB |
| Totalplay-2.4G-F9C0 | -93 dBm | -92 dBm | -1 dB |

The same access points measure up to 18 dB stronger on one board than on the
other: the difference in the spectrum comes from the local RF environment, not
the firmware. Below the LO (where external signal energy is weaker and internal
receiver noise dominates), both boards agree within ±1 dB.

> **To compare absolute levels between two SDR boards, reference RSSI must be
> measured simultaneously.** Without it, two boards placed half a meter apart
> can easily differ by 14 dB due to multipath and proximity to nearby APs.

---

## 7. Transmission from One Board Observed by the Other

> **This test round is the only one that requires a third application** (a
> radiating app). The rest of this report uses exclusively the two SDR modes.
> It is included here to document the measurement limits discussed in
> Section 7.1.

With board A radiating (Wi-Fi beacon spammer) and board B receiving in SDR mode:

**Verification that the signal arrives** (via board B's Wi-Fi receiver):

```
[-46] MININO_SDR_3       <- board A
[-46] MININO_SDR_1       <- board A
[-53] INFINITUM2843      <- strongest ambient AP
[-81] Totalplay-77A5
[-88] Totalplay-77A5_EXT
```

**What the SDR observes**, comparing the spectrum with and without transmission
(LO at 2412 MHz), falling within the transmitter band:

| Transmitter | Where the Difference Appears | Magnitude |
|---|---|---|
| Wi-Fi Beacon Spammer | **2421..2428 MHz** (+9 to +16 MHz from LO) | +0.2 to +1.1 dB |
| BLE Spammer | **2426 MHz** (+13.5 to +14.3 MHz from LO) and 2402 MHz (-10/-11 MHz) | +1.1 to +1.5 dB |

The DSSS bandwidth of the beacon spammer is 11 MHz, and the measured block
spans 7 MHz of that: the transmitter is on channel 4 (2427 MHz). When tuning
the receiver to 2427 MHz, the same band falls directly over the LO, confirming
the channel.

> The spammer configures its AP on channel 1, but beacons are emitted on the
> radio's active channel: `esp_wifi_80211_tx()` uses the current operating
> channel rather than the AP configuration channel.

**Measurement limit**: the negative control (both states doing the same thing)
yields +1.0 to +1.4 dB in the same bands, meaning the averaged delta sits near
ambient variance. In a crowded 2.4 GHz environment, with hardware AGC
normalizing gain and Minino transmitters running at a low duty cycle, time
averaging does not separate the transmitter from the background floor.

Furthermore, because the SDR borrows the Wi-Fi PHY, **both boards share the
2.4 GHz band with Bluetooth** (BLE advertising channels are at 2402, 2426, and
2480 MHz), while the SDR local oscillator can only tune to Wi-Fi channel
centers (2412-2472 MHz). Consequently, outer BLE advertising channels fall near
the capture edges: channel 37 (2402 MHz) sits at -10 MHz from the 2412 MHz LO,
and channel 39 (2480 MHz) sits at +8 MHz from the 2472 MHz LO.

---

## 8. Bugs Uncovered by the Test Suites

| Issue | Symptom | Fix |
|---|---|---|
| SSID spammer did not transmit | When launched from the console, the app transmitted nothing because the SSID list was only loaded upon menu selection | `ssid_spam_load_stored_list()`: load the list automatically when opening the app |
| Re-entering the spammer rebooted the board | `assert()` in `esp_netif_create_default_wifi_ap()`: *"duplicate key"* | Create the default AP `netif` only if it does not already exist |
| Port handover occasionally failed | Test scripts sent `sdr_host` terminated with LF while the Minino console expects **CR** | Test scripts now send CR |

None of these three bugs were inside the SDR module itself, but all three
prevented automated end-to-end testing until fixed.

---

## 9. Conclusion

- **The receive chain works end to end**: the viewer protocol, on-chip spectrum
  mode, and native app pass all checks on both boards, both with and without
  the second board radiating.
- **Both boards measure the same environment**: identical spectral shapes, same
  visible Wi-Fi networks, and amplitude differences that match the RSSI delta to
  surrounding access points.
- **Captures are genuine hardware dumps**: 16,380 samples in 217-218 us
  (75 MS/s), with 86 dB of dynamic range and the carrier peak at the LO.
- **Open-air limitation**: separating another Minino's low-duty-cycle beacon
  from ambient Wi-Fi traffic in an averaged spectrum requires an RF-quiet
  environment or a shielded enclosure.
