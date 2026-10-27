# Minino SDR: Test Results and Getting Started Guide

> User and getting started guide. The full test report is available in
> [`sdr-resultados.md`](./sdr-resultados.md) and the packet wire format
> is documented in [`sdr-protocolo.md`](./sdr-protocolo.md); the technical
> design reference (how it works, why it is built this way, and what broke
> along the way) is in [`esp-sdr-integration.md`](./esp-sdr-integration.md);
> the branch history is in
> [`changes-refactor-system-overhaul.md`](./changes-refactor-system-overhaul.md).
>
> - **Test Date:** 2026-10-21
> - **Boards:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` and rev v0.1
>   `7c:2c:67:42:84:a0`, both with 8 MB flash
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`

---

## 1. Overview on One Page

The ESP32-C6 features a hardware **dump receive engine**, a dedicated SRAM block (`0x40840000`) where the radio directly deposits raw demodulated I/Q samples at **80 MS/s** (with decimation to 40 and 16 MS/s), in addition to raw 802.11 frame injection and PHY synthesizer continuous wave (CW) carrier modes. With the native application (`Applications > SDR`), **4 operating modes** are available on the device (switchable with the **BOOT** button or by holding **RIGHT**):

| Mode | OLED Header | Description and Purpose |
|---|---|---|
| **1. RX I/Q Capture** | `MININO SDR [IDL]` / `[RX]` | Captures bursts of 8,192 I/Q samples at 80 MS/s, calculates real-time physical RSSI (`dBm`), and saves them to the microSD card (`/sdcard/sdrcapXX.iq`). |
| **2. Raw 802.11 Injection** | `MININO SDR [TX:PKT]` | Injects bursts of 802.11 Probe Request packets (`MININO-SDR-TX`) on the tuned channel (`2412-2484 MHz`, Channels 1 to 14). |
| **3. Continuous Wave (CW) Carrier** | `MININO SDR [TX:CW]` | Emits a continuous wave (CW) carrier tone via the PHY synthesizer (`c6_sdr_tx_carrier_start`) with an automatic 30-second safety cutoff. |
| **4. WebSDR Host Mode** | `MININO SDR [HOST]` | Connects the board over USB to the **ESPARGOS** web analyzer (<https://espargos.net/espsdr/app/>) to display waterfall plots, on-chip FFT spectrum (`SPEC`/`SPC1`), and I/Q captures (`CAP16`/`CAP20`) with LNA gain control (`0-60 dB`). |

Additionally, while inside `Applications > SDR`, Minino **blocks USB DTR/RTS auto-reset** (`usb_uart_chip_rst_dis = 1`) and **automatically activates `[HOST]` mode** as soon as <https://espargos.net/espsdr/app/> sends the `SYNC` command.

---

## 2. Getting Started

### 2.1 Build Variants

The capture bank consumes an entire SRAM block (128 KiB), and the linker
script trims the memory layout so the firmware image fits below
`0x40840000`. With the full Minino image (Bluetooth, Zigbee, and Thread
included), it does not fit: it exceeds the limit by 45 KB. For this reason,
there are **two firmware images**:

```bash
cd firmware
make builds        # Standard board image: all protocols included, SDR disabled
make build-sdr     # SDR image: Zigbee and Thread excluded, SDR enabled
```

| Variant | Size | Flash Headroom |
|---|---|---|
| Standard (`profiles/minino`) | 3,377 KiB | 8.7% |
| SDR (`profiles/sdr`) | 2,561 KiB | 32% |

### 2.2 Flashing

```bash
cd firmware
idf.py -B build-sdr -p /dev/ttyACM0 flash     # or ttyACM1
```

Or directly using `esptool`:

```bash
esptool.py --chip esp32c6 -p /dev/ttyACM0 write_flash \
  0x0    build-sdr/bootloader/bootloader.bin \
  0x8000 build-sdr/partition_table/partition-table.bin \
  0x15000 build-sdr/ota_data_initial.bin \
  0xa0000 build-sdr/minino.bin
```

Requirements: an **8 MB** board with the 8 MB partition table
(`partitions.csv`). In the standard board image, SDR is **not present**: the
code compiles, but there is no capture bank allocated behind it and the menu
does not display the entry.

### 2.3 Verification

```bash
python3 tools/test_esdr_web.py /dev/ttyACM0    # Browser protocol suite
python3 tools/test_esdr_spec.py /dev/ttyACM0   # Spectrum mode suite
```

> Both test scripts hand over the serial port using `sdr_host` automatically.
> When triggered manually, the Minino console terminates lines with **CR**, not
> LF: `sdr_host` + CR.

---

## 3. Three Ways to Use It

### 3.1 Native App and Its 4 Modes

`Applications > SDR` (or `sdr_capture` in the serial console):

| Button | Action |
|---|---|
| `UP` / `DOWN` | Tunes ±5 MHz, from `2412` to `2484 MHz` (Channels 1 to 14) |
| `RIGHT` (short press) | Executes the active mode action: captures an I/Q burst in `[RX]`, sends packets in `[TX:PKT]`, or toggles the carrier in `[TX:CW]` |
| `BOOT` or `Hold RIGHT` | Cycles through the 4 modes: `RX Capture` -> `TX 802.11 Raw` -> `TX CW Tone` -> `Web Host Mode` |
| `LEFT` | Stops any active transmission or Host session and exits to the menu, returning radio control to Minino |

In the serial console, the test hook allows performing the same actions without physical buttons:

```
minino> sdr_capture
minino> press right     # burst 1: 8192 words in 218 us
minino> press up        # Tuned to 2417 MHz (CH 2)
minino> press left      # Exits and releases the Wi-Fi driver
```

Capture files are written to `/sdcard/sdrcap00.iq` (one 32-bit word per
sample, I in bits 9-0 and Q in bits 19-10). **Without a microSD card, capture
still works, but data is not saved to disk**: the app reports this on screen and
in the serial log (`Cap OK: not saved`).

### 3.2 Host Mode and the ESPARGOS Web Viewer

You can connect Minino to the official **ESPARGOS WebSDR** web application (<https://espargos.net/espsdr/app/>) in **three ways**:

1. **Automatic activation from the SDR app (recommended):** Enter `Applications > SDR` on the device, open <https://espargos.net/espsdr/app/> in Chrome/Edge/Brave, and click **Connect**. Upon receiving `SYNC` over USB, Minino automatically switches to `MININO SDR [HOST]` mode, pauses the serial console, and begins streaming spectrum/I/Q data without rebooting the board.
2. **Manual on-screen selection:** Inside `Applications > SDR`, press **BOOT** (or hold **RIGHT**) until reaching `MININO SDR [HOST]` (`Web: espargos.net`), then connect from the browser.
3. **Via serial console command:**
```bash
minino> sdr_host
Serving the ESP-SDR protocol on the USB port.
Open https://espargos.net/espsdr/app/ and connect to this board.
The console comes back when the viewer sends RELEASE, or type MININO.
```

From that point on, the viewer speaks the ESP-SDR *burst* firmware line
protocol (`SYNC`, `INFO`, `CAPS`, `FREQ`, `GAIN`, `CAP16`, `CAP20`,
`SPEC`...). The console stops reading the serial port for the duration of the
session, and the log level drops to `ESP_LOG_NONE` so log messages do not
corrupt binary capture streams. The viewer sends `RELEASE` when closing; you
can also exit by pressing **LEFT** or **BOOT** on Minino, or by sending
`MININO`.

> The **"Flash Firmware..."** button in the web viewer flashes the stock
> ESPARGOS firmware over Minino: ignore that button.

### 3.3 Automated Test Suites

| Tool | What It Tests |
|---|---|
| `tools/test_esdr_web.py` | Browser protocol byte by byte: handshake, regular expressions, CRC32 verification, and I/Q unpacking |
| `tools/test_esdr_spec.py` | Spectrum mode: profiles, `SPC1` frames, CRC verification, and stream termination |
| `tools/test_esdr_app.py` | Native on-board app: burst capture, channel tuning, and clean exit |
| `tools/test_esdr_pair.py` | Both SDR applications running simultaneously across both boards, plus side-by-side profile comparison |
| `tools/test_esdr_radio.py` | Over-the-air test: one board transmits while the other receives using the SDR |

---

## 4. Test Results

### 4.1 Both Boards, All Three Suites

Both boards run the SDR firmware variant and pass all three test suites
**while the other board is actively radiating**:

| Board | `test_esdr_web.py` | `test_esdr_spec.py` | `test_esdr_app.py` |
|---|---|---|---|
| `48:f6:ee:c2:a2:14` (v0.2) | **56 of 56** | **28 of 28** | **16 of 16** |
| `7c:2c:67:42:84:a0` (v0.1) | **56 of 56** | **28 of 28** | **16 of 16** |

And all 20 host test suites: `make tests` -> **0 failures**.

The 84 on-board checks cover: device identity (`C6SDR 6 burst 16380`),
capabilities expected by the viewer and those that must remain hidden, the
2412-2472 MHz tuning range, limits (sample rates, bit depths, gain), frequency
tuning (accepting 2417 MHz and rejecting 2000 MHz), 16,380-sample captures at
8 and 10 bits with CRC validation, all three sample rates (80, 40, and
16 MS/s), spectrum dynamic range, `RELEASE`, the `MININO` escape command, and
console restoration.

### 4.2 Spectrum Profile Across Both Boards

Snapshot from spectrum mode (LO at 2412 MHz, 40 MS/s, 1024 bins,
1,427 averaged frames). Both profiles match in shape: the wide hump of Wi-Fi
channel 1 (2401-2423 MHz) centered at the LO, with the noise floor 10 MHz
further out.

```
   board 48:f6:ee           board 7c:2c:67
   peak 113  median 58      peak 116  median 58
   min  8                   min  8
   LO (bin 0)  74.0 dB      LO (bin 0)  74.4 dB

   MHz from LO  dB          MHz from LO  dB
     -12      49.5            -12      50.2
     -10      56.6            -10      55.7
      -5      61.7             -5      60.5
      -2      68.2             -2      65.0
      +0      74.0             +0      74.4
      +2      71.1             +2      75.2
      +5      69.1             +5      71.6
     +10      65.0            +10      69.8
     +12      48.7            +12      54.6
     +15      22.5            +15      28.3
     +19      10.8            +19      12.9
```

The dynamic range of a burst computed on the host is **86 to 89 dB**, with the
peak located at the LO (where the carrier of the tuned channel resides).

### 4.3 Native On-Board App

```
I (105901) esp_sdr_module: Entering native SDR I/Q capture app...
I (106154) c6_sdr_rx: Initializing C6 native SDR RX engine (buffer at 0x40840000)...
I (122537) esp_sdr_module: burst 2: 16380 words in 218 us
I (130544) c6_sdr_rx: Tuned to 2417 MHz (CH 2)
I (133649) c6_sdr_rx: The Wi-Fi driver was started for the capture and is free again
```

16,380 words (65 KiB) in 218 us corresponds to **75 MS/s**, consistent with the
80 MS/s dump engine rate.

### 4.4 Over-the-Air Test (Separate Round)

> This is the **only** measurement that requires a third application: a
> radiating transmitter app. Everything else in this document uses only the two
> SDR modes. For tests involving exclusively the two SDR modes, see Section 5.

One board transmits while the other receives using the SDR. First, verification
that the signal **arrives**: the Wi-Fi receiver on the receiving board detects
the beacons from the transmitting board as the strongest signals in the air.

```
board B, wifi_scan, with board A transmitting:

[-46] MININO_SDR_3       <- board A
[-46] MININO_SDR_1       <- board A
[-53] INFINITUM2843      <- strongest ambient AP
[-81] Totalplay-77A5
...
```

Second, what the SDR observes: the difference between the spectrum with
transmission active versus inactive falls **within the transmitter band**.

| Transmitter | Where the Difference Appears (LO at 2412 MHz) | Magnitude |
|---|---|---|
| Wi-Fi Beacon Spammer | **2421..2428 MHz** (+9 to +16 MHz from LO) | +0.2 to +1.1 dB |
| BLE Spammer | **2426 MHz** (+13.5 to +14.3 MHz from LO) and 2402 MHz (-10/-11 MHz) | +1.1 to +1.5 dB |

The DSSS bandwidth of the beacon spammer is 11 MHz, and the measured block
(+9..+16 MHz) spans 7 MHz of that: the transmitter is on channel 4 (2427 MHz).
When tuning the receiver to 2427 MHz, the same band falls directly over the
LO, confirming the channel.

> The spammer configures its AP on channel 1, but beacons are transmitted on
> whichever channel the radio is currently using: `esp_wifi_80211_tx()` uses
> the active operating channel rather than the AP configuration channel.

### 4.5 Honest Measurement Limits

The difference with transmission active (+0.2 to +1.1 dB) is **on the same
order as the negative control** (both states doing the same thing: +1.0 to
+1.4 dB in the same bands). In a congested 2.4 GHz environment, with the
receiver AGC normalizing gain and Minino transmitters operating at a low duty
cycle (one beacon every 100 ms), the transmitter signal does not separate from
the background average.

What clearly rises above the noise floor is the **peak** of a narrow burst:
with the BLE spammer active, a capture burst that catches an advertising packet
measures **+10 dB above the floor** on advertising channel 37 (2402 MHz),
right where expected.

Unambiguous attribution in averaged spectra requires an RF-quiet environment or
an anechoic/shielded enclosure rather than additional open-air measurements.
This is documented in [`esp-sdr-integration.md` Section 9.6](./esp-sdr-integration.md).

---

## 5. Both SDR Applications Across Both Boards

> With both Minino boards on the bench, each running **one** of the two SDR
> applications and **nothing else**: the native app (I/Q burst capture) on one
> board and spectrum mode (on-chip FFT) on the other, and vice versa. Both
> boards sample the same RF environment, so their results must be mutually
> consistent.

The unified test runner:

```bash
python3 tools/test_esdr_pair.py <mode> <port_1> <port_2>
```

| Mode | Action |
|---|---|
| `app+spec` | Runs the native app on the first board and spectrum mode on the second, **simultaneously** |
| `spec+spec` | Runs spectrum mode on both boards simultaneously and compares both profiles |
| `app+app` | Runs the native app on both boards simultaneously |

### 5.1 Test Matrix

The two boards: `48:f6:ee:c2:a2:14` (rev v0.2) and `7c:2c:67:42:84:a0`
(rev v0.1), both running the SDR build variant.

| Assignment | Board A | Board B | Result |
|---|---|---|---|
| 1 | Native app | Spectrum mode | App: 3 bursts of 16,380 words in 217-218 us, tuning 2412 <-> 2417 MHz, clean exit; Spectrum: 1,416 frames with valid CRC (peak 116) |
| 2 | Spectrum mode | Native app | Spectrum: 1,399 frames (peak 112); App: 3 bursts of 16,380 words in 217-218 us |
| 3 | Native app | Native app | Both simultaneously: 3 bursts each (one took 557 us due to a driver hiccup) |
| 4 | Spectrum mode | Spectrum mode | 1,413 and 1,402 frames, with both profiles compared band by band |

Individual test suites on both boards:

| Suite | 48:f6:ee | 7c:2c:67 |
|---|---|---|
| `tools/test_esdr_app.py` (native app) | **16 of 16** | **16 of 16** |
| `tools/test_esdr_spec.py` (spectrum mode) | **28 of 28** | **28 of 28** |

Native app check-by-check verification: app launch, capture engine startup,
initial tuning to 2412 MHz, microSD response, 16,380-word burst size, capture
rate near 80 MS/s (**75.1 and 75.5 MS/s** measured), file write to SD, UP and
DOWN tuning between 2417 and 2412 MHz, burst counter increment, app exit,
engine shutdown, radio release back to Minino, and console responsiveness.

### 5.2 Both Profiles in the Same Environment

Both boards measuring the spectrum **simultaneously** (LO at 2412 MHz,
40 MS/s, 1024 bins, ~1,400 frames each):

```
         band    board 7C:2C:67   board 48:F6:EE     diff
       -20 MHz            8.9            9.0    -0.1
       -16 MHz           19.6           19.0    +0.6
       -12 MHz           52.7           52.7    +0.0
        -8 MHz           59.5           60.0    -0.5
        -4 MHz           62.2           62.3    -0.0
        +0 MHz           69.9           67.8    +2.0
        +2 MHz           82.3           67.6   +14.7
        +6 MHz           80.7           67.7   +13.0
       +10 MHz           73.9           61.8   +12.2
       +14 MHz           40.6           27.3   +13.3
       +18 MHz           18.8           11.2    +7.6
```

**The spectral shape is identical** on both boards (the Wi-Fi channel 1 hump,
same noise floor, same shoulders), while **the amplitude level differs**: up to
14.7 dB difference above the LO.

The difference is not caused by the boards, but by **their physical position**.
Running `wifi_scan` on each board at the same instant confirms this:

| Network | 48:f6:ee | 7c:2c:67 | Difference |
|---|---|---|---|
| INFINITUM2843 | -45 dBm | -58 dBm | **+13 dB** |
| Totalplay-77A5 | -69 dBm | -87 dBm | **+18 dB** |
| Club_Totalplay_WiFi | -71 dBm | -87 dBm | +16 dB |
| Totalplay-77A5_EXT | -83 dBm | -95 dBm | +12 dB |

The same access points show 13 dB higher signal strength on one board than on
the other: the spectrum difference (+12 to +14 dB) comes from the environment,
not the firmware. Below the LO, both boards match within ±1 dB.

> Takeaway for measurements: **two SDR boards do not produce directly
> comparable amplitude levels unless co-located**. Either measure reference
> RSSI simultaneously or use a shielded enclosure.

### 5.3 What Was Excluded

Not a single radiating application (no spammer, no Flock simulator, no active
scanner) was active during this test: the only two applications running were
the two SDR modes (burst capture and spectrum). The `wifi_scan` in Section 5.2
serves purely as an ambient *reference* (measuring surrounding signal levels),
not a transmitter.

## 6. Issues Uncovered During Testing

| Issue | Symptom | Fix |
|---|---|---|
| SSID spammer did not transmit | Launched from the console, the app transmitted nothing because the SSID list was only loaded when selecting a menu item | `ssid_spam_load_stored_list()`: load the list automatically on app startup |
| Re-entering the spammer rebooted the board | `assert()` in `esp_netif_create_default_wifi_ap()`: *"duplicate key"* | Create the default AP `netif` only if it does not already exist |
| Serial port handover occasionally failed | Test scripts sent `sdr_host` terminated with LF while the Minino console expects **CR** | Test scripts now send CR |

---

## 7. Quick Reference

### 7.1 Console Commands

| Command | Description |
|---|---|
| `sdr_capture` | Opens the native SDR app |
| `sdr_host` | Hands the serial port over to the ESPARGOS viewer protocol |
| `launch ssid_spam` | Opens the Wi-Fi beacon spammer (`press down` + `press right` starts the attack) |
| `launch spam_all` | Opens the BLE spammer |
| `launch flock_sim` | Opens the Flock simulator |
| `wifi_scan` | Lists visible Wi-Fi networks (useful to verify whether a transmission is arriving) |
| `press <up\|down\|left\|right>` | Injects a button press event |
| `display` | Reports the current display panel and active app status |

### 7.2 Viewer Wire Protocol

```
SYNC <nonce>   -> SYNC <nonce>
INFO           -> C6SDR 6 burst 16380
CAPS           -> CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16 ...
FREQ <MHz>     -> OK
LIMITS?        -> LIMITS {"gain":[0,0,1],...}
RANGE?         -> RANGE 2412 2472 1
CAP16 <n> <r>  -> DATA <n> <crc32hex> <us>  +  2*n bytes
CAP20 <n> <r>  -> DATA <n> <crc32hex> <us>  +  ceil(20*n/8) bytes
SPEC <ms> <step> <units> <det> <rate> <bins> -> SPEC <bins> <rate> <bins> <mhz> + frames
RELEASE        -> OK      (console returns)
MININO         -> OK      (Minino-specific escape command)
```

### 7.3 Known Limitations

| Limitation | Reason |
|---|---|
| Receive-only I/Q dump | The ESP32-C6 has no arbitrary waveform TX DAC; the dump engine is an RX peripheral |
| Automatic hardware AGC in dump mode | Hardware performs AGC; the `GAIN` capability is hidden when fixed by hardware |
| No analog bandwidth filter control | Not implemented in hardware |
| No 5 GHz support | The ESP32-C6 radio is 2.4 GHz only |
| Manual console port handover | When using console commands, the REPL echoes input whereas the viewer requires a clean stream |
| Viewer "Flash Firmware..." overwrites Minino | Must be ignored |
