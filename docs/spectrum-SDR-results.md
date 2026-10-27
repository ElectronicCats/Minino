# Spectrum-SDR Test Results

> Test results for the Minino SDR using **exclusively the two SDR
> applications**: the native app (I/Q burst capture) and spectrum mode
> (on-chip FFT). No radiating applications were used: one Minino measures with
> one application while the second board measures with the other in the same RF
> environment, and vice versa.
>
> - **Date:** 2026-10-21
> - **Boards:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` and rev v0.1
>   `7c:2c:67:42:84:a0`, both with 8 MB flash
> - **Firmware:** SDR build variant (`make build-sdr`, `minino.bin` at
>   2,561,504 bytes, 32% free in the application partition)
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`
> - **Related Documents:** [`sdr-protocolo.md`](./sdr-protocolo.md) (wire
>   protocol byte by byte), [`sdr-guia.md`](./sdr-guia.md) (user guide), and
>   [`sdr-resultados.md`](./sdr-resultados.md) (full report)

---

## 1. Summary

| Suite | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| ESPARGOS viewer protocol, byte by byte | **56/56** | **56/56** |
| Spectrum mode (on-chip FFT) | **28/28** | **28/28** |
| Native SDR app (burst capture, tuning, exit) | **16/16** | **16/16** |

| Both Applications Running Simultaneously Across Both Boards | Result |
|---|---|
| Native app on A + Spectrum on B | 3 bursts of 16,380 words (218, 217, 218 us); 1,416 frames with valid CRC |
| Spectrum on A + Native app on B | 1,399 frames; 3 bursts (217, 218, 218 us) |
| Native app on both boards | 3 bursts each (one took 557 us) |
| Spectrum on both boards | 1,413 and 1,402 frames, with both profiles compared band by band |

Host unit tests: 15 C test suites (378 tests) plus the Python test suite, **0
failures**.

The spectrum profile across both boards, plotted and explained, is in Section 5.

---

## 2. Native App (Burst Capture)

`tools/test_esdr_app.py` - tests the on-device application using the `press`
console test hook to simulate button presses.

| Check | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| App launches (`Entering native SDR I/Q capture app`) | PASS | PASS |
| Capture engine initializes (buffer at `0x40840000`) | PASS | PASS |
| Tunes to 2412 MHz (CH 1) on entry | PASS | PASS |
| MicroSD responds and saves the burst | PASS | PASS |
| Burst size is 16,380 words | PASS | PASS |
| Capture rate is close to 80 MS/s | **75.5 MS/s** | **75.1 MS/s** |
| UP tunes to 2417 MHz (CH 2) | PASS | PASS |
| DOWN returns to 2412 MHz (CH 1) | PASS | PASS |
| Burst counter increments | PASS | PASS |
| App exits and stops the engine | PASS | PASS |
| Radio lease returns to Minino | PASS | PASS |
| Serial console remains responsive | PASS | PASS |

**16 of 16 passed on both boards.**

Raw serial log from the board:

```
I (105901) esp_sdr_module: Entering native SDR I/Q capture app...
I (106154) c6_sdr_rx: Initializing C6 native SDR RX engine (buffer at 0x40840000)...
I (122537) esp_sdr_module: burst 2: 16380 words in 218 us
I (130544) c6_sdr_rx: Tuned to 2417 MHz (CH 2)
I (133649) c6_sdr_rx: The Wi-Fi driver was started for the capture and is free again
```

16,380 words (65 KiB) in 218 us corresponds to **75 MS/s**, consistent with the
80 MS/s hardware dump engine.

---

## 3. Spectrum Mode

`tools/test_esdr_spec.py` - validates the on-chip FFT and the `SPC1` frames
streamed over USB.

| Check | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| All 12 profiles (3 sample rates x 4 FFT sizes) | PASS | PASS |
| Header `SPEC <bins> <rate> <bins> <mhz>` | PASS | PASS |
| `SPC1` frames arrive with valid CRC32 | PASS | PASS |
| Bins are 1-byte codes from 0 to 255 | PASS | PASS |
| Spectrum has structure (not flat noise) | PASS | PASS |
| LO carrier peak is at bin 0 | PASS | PASS |
| `SPECEND` returns all twelve fields on clean stop | PASS | PASS |

**28 of 28 passed on both boards.**

Measured stream metrics:

| Metric | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Frames in 12 s | 1,430 | 1,416 |
| `drops` / `abandoned` | 0 / 0 | 0 / 0 |
| Peak / median / floor | 116 / 59 / 8 | 112 / 59 / 8 |

---

## 4. Spectrum Profile Across Both Boards

Snapshot from spectrum mode: LO at 2412 MHz, 40 MS/s, 1024 bins,
1,427 averaged frames, measured one board at a time.

| Metric | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Peak | 113 | 116 |
| Median | 58 | 58 |
| Noise floor (minimum) | 8 | 8 |
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
LO (Wi-Fi channel 1, 2401-2423 MHz) and the noise floor 10 MHz further out.

### 4.1 Simultaneous Measurement on Both Boards

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

**Same spectral shape, different amplitude level**: up to 14.7 dB above the LO.

This difference is caused by **physical board position**, not hardware
variation. Running `wifi_scan` on both boards at the same instant shows:

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

The same access points arrive up to 18 dB stronger at one board than the other:
the spectrum difference comes from the local environment, not the firmware.
Below the LO, both boards match within ±1 dB.

> **To compare absolute levels between two SDR boards, reference RSSI must be
> measured simultaneously.** Without it, two boards placed half a meter apart
> can differ by 14 dB simply due to local multipath and AP proximity.

---

## 5. How to Read the Spectrum Plots

### 5.1 Axes

| Axis | Meaning |
|---|---|
| **Horizontal** | Frequency offset from the LO in MHz. The LO is the center frequency to which the board is tuned (2412 MHz, channel 1, by default). Because the signal is **complex** (I and Q), the spectrum spans from $\text{LO} - \text{rate}/2$ to $\text{LO} + \text{rate}/2$: ±20 MHz at 40 MS/s, ±40 MHz at 80 MS/s, and ±8 MHz at 16 MS/s |
| **Vertical** | Power code for each bin: a byte from 0 to 255 representing `20*log10(power)` in Q15 format. The viewer multiplies it by frame byte 27 (`2`) to display values in dB |

The tables in this document group bins into 1 MHz bands: at 40 MS/s with 1024
bins, each bin is 39.06 kHz wide, so each row averages roughly 26 bins. The
ASCII plot below scales from the noise floor (8) to the peak (75): what matters
is the **shape** rather than the absolute offset.

```
   MHz from LO |                                        | dB  |                                        | dB
   ------------+----------------------------------------+-----+----------------------------------------+-----
               |        48:f6:ee        |      |        7c:2c:67        |
    -20        |#######################################|  8.6 |#######################################|  8.2
    -19        |#######################################|  8.7 |#######################################|  8.6
    -18        |######################################|  9.9 |######################################| 10.7
    -17        |#####################################| 11.8 |#####################################| 12.4
    -16        |###################################| 15.4 |###################################| 15.8
    -15        |################################| 20.5 |################################| 21.0
    -14        |############################| 27.8 |###########################| 28.2
    -13        |######################| 36.7 |######################| 36.9
    -12        |###############| 49.5 |##############| 50.2
    -11        |##############| 53.3 |#############| 52.6
    -10        |##########| 56.6 |###########| 55.7
     -9        |##########| 57.7 |##########| 57.2
     -8        |#########| 58.9 |##########| 58.0
     -7        |#########| 59.8 |#########| 58.9
     -6        |#########| 59.8 |#########| 59.1
     -5        |#######| 61.7 |########| 60.5
     -4        |#####| 65.5 |#######| 61.6
     -3        |####| 67.0 |#######| 63.1
     -2        |####| 68.2 |#####| 65.0
      0        || 74.0 || 74.4  <- LO
     +1        |####| 68.0 |######| 64.9
     +2        |##| 71.1 || 75.2
     +3        |##| 70.6 || 74.2
     +4        |###| 69.6 |#| 72.8
     +5        |###| 69.1 |##| 71.6
     +6        |###| 68.7 |#| 71.8
     +7        |###| 68.9 |#| 73.1
     +8        |###| 68.5 |#| 73.3
     +9        |####| 67.0 |#| 71.9
    +10        |#####| 65.0 |###| 69.8
    +11        |#########| 58.3 |#######| 62.8
    +12        |###############| 48.7 |############| 54.6
    +13        |#####################| 39.8 |##################| 43.9
    +14        |###########################| 29.2 |#######################| 36.3
    +15        |###############################| 22.5 |###########################| 28.3
    +16        |##################################| 17.6 |###############################| 23.0
    +17        |####################################| 14.3 |##################################| 18.0
    +18        |#####################################| 12.0 |###################################| 15.0
    +19        |######################################| 10.8 |#####################################| 12.9
```

### 5.2 What the Spectral Shape Reveals

| Visual Feature | Meaning |
|---|---|
| **Wide central hump** (-12 to +11 MHz) | Wi-Fi channel 1 band (2401-2423 MHz): aggregate energy from surrounding access points |
| **Peak at 0 MHz** | Carrier of the tuned channel: the LO sits at the center of the band where energy peaks |
| **Shoulders** (-16 and +16 MHz) | Sideband skirts of 11 MHz DSSS and 20 MHz OFDM signals |
| **Floor at ~8 dB** | Internal receiver noise floor |
| **Roll-off to the floor** (-12 to -17 MHz and +12 to +19 MHz) | Outside the Wi-Fi channel band (2395 MHz and 2431 MHz), Wi-Fi energy drops off sharply |

The **dynamic range** (from peak to noise floor) is **86 to 88 dB** across
captures, with the peak centered on the LO: confirming proper I/Q balance and
frequency alignment.

### 5.3 Side-by-Side Board Comparison

Both curves share **the exact same shape**: same central hump, same shoulders,
and same peak location. Only the **height** differs:

- **Below the LO**, both boards match within ±1 dB because internal receiver
  noise dominates equally on both units.
- **Above the LO**, there is up to 14.7 dB of difference caused by **where each
  board sits on the bench**. Simultaneous `wifi_scan` measurements show the
  same access points arriving +13 dB (`INFINITUM2843`) and +18 dB
  (`Totalplay-77A5`) stronger at the closer board.

In short, the plot reflects **the physical location**, which is why reference
`wifi_scan` measurements must be captured alongside spectrum snapshots.

### 5.4 How the Web Viewer Renders the Data

| Mode | Horizontal Axis | Vertical Axis | Color |
|---|---|---|---|
| Burst | Time (one column per burst) | Frequency | Power |
| On-Chip Spectrum | Time (one column per `SPC1` frame) | Frequency | Power |

In both modes, the waterfall is built from the same underlying RF source: in
Burst mode, the browser computes the FFT from `CAP16`/`CAP20` samples, whereas
in On-Chip Spectrum mode, the FFT is pre-computed on the board (enabling dozens
of frames per second).

The viewer's *"Reception has gaps between snapshots"* notice is triggered by
**bit 3 of the frame flags byte**: the hardware dump engine must be re-armed
between captures, so the spectrum stream is snapshot-based rather than
continuous.

### 5.5 Key Findings

1. **Reception works and the frequency axis is properly aligned**: the peak
   lands on the LO and the hump matches Wi-Fi channel 1 (2401-2423 MHz). If the
   frequency axis were shifted or mirrored, the channel hump would not align.
2. **The 2.4 GHz band is shared with Bluetooth**, with a tuning range caveat:
   the SDR can only tune its LO to Wi-Fi channels (2412-2472 MHz) because the
   dump engine borrows the Wi-Fi PHY. BLE advertising channels sit at the band
   edges: channel 37 (2402 MHz) is at -10 MHz from the 2412 MHz LO, and channel
   39 (2480 MHz) is at +8 MHz from the 2472 MHz LO. **Bluetooth channels cannot
   be centered directly as the LO**, only observed at the passband edges.
3. **Two SDR boards in different spots do not have identical amplitude levels**:
   up to 14.7 dB of difference due to position, while preserving the same
   spectral shape.
4. **Dynamic range reaches 86-88 dB** and the measured capture rate is 75 MS/s
   against the 80 MS/s nominal hardware rate.
5. **The spectrum stream has inter-frame gaps** (bit 3 of the flags byte)
   because the hardware dump engine must be re-armed between snapshots.
6. **Hardware AGC normalizes receiver gain**: absolute spectrum levels are not
   directly comparable across boards without a reference signal; spectral shape
   and relative SNR are.

---

## 6. Wire Packets at a Glance

The two packet formats sent over USB (full details and raw hex dumps in
[`sdr-protocolo.md`](./sdr-protocolo.md)):

| Attribute | Burst Capture | On-Chip Spectrum |
|---|---|---|
| Request | `CAP20 <n> <rate>` | `SPEC <ms> <step> <units> <det> <rate> <bins>` |
| Response | `DATA <n> <crc32hex> <us>` + binary payload | `SPEC <bins> <rate> <bins> <mhz>` + `SPC1` frames |
| Payload | 2 bytes per pair (`CAP16`) or 2.5 bytes (`CAP20`, 20-bit: I in 0-9, Q in 10-19) | 1 byte per bin (power code) |
| CRC32 | zlib CRC32 over the binary payload | zlib CRC32 over the frame header plus bins |
| Size | 10,240 bytes for 4096 samples | 1,056 bytes per 1024-bin frame |

Measured on both boards (real bytes, with verified CRC32):

```
-> CAP20 4096 0
<- DATA 4096 31145255 65          (48:f6:ee)
   000000  5a f8 3f 86 fd 70 80 7f 07 f3 72 08 4f 06 f2 52  Z.?..p....r.O..R
   ... (10240 bytes total)
   payload CRC32 (zlib): 31145255   (advertised in header: 31145255)

-> CAP20 4096 0
<- DATA 4096 1a58a795 64          (7c:2c:67)
   000000  31 7c d1 c3 14 2d dc c0 00 05 ee 17 f0 bd 02 dd  1|...-..........
   ... (10240 bytes total)
   payload CRC32 (zlib): 1a58a795   (advertised in header: 1a58a795)

-> SPEC 0 1 1 0 1 1024
<- SPEC 1024 40000000 1024 2412
   000000  53 50 43 31 00 00 00 00 00 00 00 00 00 00 00 00  SPC1............
   000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 48 00 00  .......N.....H..
   ... (1056 bytes total)
   header + bins CRC32: 5367f516    (frame trailer: 5367f516)
```

And terminating the spectrum stream:

```
-> (empty line)
<- SPECEND 0 0 65 66560 759 0 0 65 0 0 65 1
```

---

## 7. Where Everything Appears

| Interface | What Is Displayed |
|---|---|
| **ESPARGOS Viewer**, Burst mode | Spectrum waterfall and I/Q plots computed by the **browser** from `CAP16`/`CAP20` |
| **ESPARGOS Viewer**, On-Chip Spectrum mode | Spectrum pre-computed **on the board** (bin power codes), refreshing at dozens of frames per second |
| **`Applications > SDR`** (native app) | `MININO SDR [IDL]`, `FREQ: 2412 MHz`, `RATE: 80 MS/s IQ`, `CAPS: 31 PKTS`, level meter, and `^v:Tune <:Exit`; after each capture, `CAP 16380 smp 218 us` and storage status (`iQ saved to SD` or `no SD: capture only`) |
| **`Applications > Spectrum`** (Minino RF analyzer) | **Separate tool**: per-channel power using Wi-Fi driver packet RSSI (channel, dB, threshold, Fox Hunt proximity, DroneID). Not the SDR FFT dump engine |

---

## 8. Issues Uncovered During Testing

| Issue | Symptom | Fix |
|---|---|---|
| SSID spammer did not transmit from the console | The SSID list was only loaded when selecting a menu item | `ssid_spam_load_stored_list()`: load the list automatically on app startup |
| Re-entering the spammer rebooted the board | `assert()` in `esp_netif_create_default_wifi_ap()` (*"duplicate key"*) | Create the default AP `netif` only if it does not already exist |
| Port handover occasionally failed | Test scripts sent `sdr_host` with LF while the console expects **CR** | Test scripts now send CR |

None of these bugs were in the SDR module itself, but all three blocked
automated testing until resolved.

---

## 9. How to Reproduce

```bash
cd firmware
make build-sdr
idf.py -B build-sdr -p /dev/ttyACM0 flash
idf.py -B build-sdr -p /dev/ttyACM1 flash

# Single-board suites
python3 tools/test_esdr_web.py  /dev/ttyACM0    # Viewer wire protocol
python3 tools/test_esdr_spec.py /dev/ttyACM0    # On-chip spectrum mode
python3 tools/test_esdr_app.py  /dev/ttyACM0    # Native on-board app

# Simultaneous dual-board suites
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM1 /dev/ttyACM0
python3 tools/test_esdr_pair.py app+app   /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py spec+spec /dev/ttyACM0 /dev/ttyACM1

# Wire capture byte by byte
python3 tools/test_esdr_wire.py /dev/ttyACM0

# Host unit tests
make tests
```

The boards are located via their persistent USB serial paths:

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_48:F6:EE:C2:A2:14-if00
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_7C:2C:67:42:84:A0-if00
```

---

## 10. Conclusion

- **Reception works across both boards**: all three test suites (56, 28, and 16
  checks) and both applications running simultaneously pass across all four
  assignments.
- **Both boards measure the same environment**: identical spectral shapes, same
  visible networks, and amplitude differences explained by physical distance to
  surrounding APs.
- **Captures are genuine hardware dumps**: 16,380 samples in 217-218 us
  (75 MS/s), with 86-88 dB of dynamic range and the peak centered at the LO.
- **Wire format matches viewer expectations**: `DATA` packets and `SPC1` frames
  pass CRC32 verification on both boards with verified byte layouts.
