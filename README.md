# Minino

![GitHub release (with filter)](https://img.shields.io/github/v/release/ElectronicCats/Minino?color=%23008000)
![GitHub actions](https://img.shields.io/github/actions/workflow/status/ElectronicCats/Minino/builds.yml)
![Static Badge](https://img.shields.io/badge/made-with_love-blue?color=%23008000)

<p align="center">
    <a href="https://github.com/ElectronicCats/Minino/wiki">
        <img src="https://github.com/ElectronicCats/Minino/assets/107638696/ad4bffb2-d428-439c-b691-60add7cfb9af" height=500>
    </a>
</p>

<p align=center>
    <a href="https://electroniccats.com/store/minino/">
        <img src="https://github.com/ElectronicCats/flipper-shields/assets/44976441/0c617467-052b-4ab1-a3b9-ba36e1f55a91" width="200" height="104" />
    </a>
    <a href="https://github.com/ElectronicCats/Minino/wiki">
        <img src="https://github.com/ElectronicCats/flipper-shields/assets/44976441/6aa7f319-3256-442e-a00d-33c8126833ec" width="200" height="104" />
    </a>
</p>

Minino is an original multiprotocol and multiband board made for sniffing, communicating, and attacking IoT (Internet of Things) devices. It was designed as a mini Cat that integrates the powerful ESP32-C6, GPS, microSD card slot, and OLED display. This board is a mini Swiss army knife for IoT security researchers, developers, and enthusiasts. 

Minino can operate across multiple wireless technologies and RF tools:
- **BLE** (Trackers scanner for AirTag, SmartTag, Tile, Chipolo & BLE Spam / HID)
- **Wi-Fi** (Sniffer, Deauthenticator, CatDoS, SSID Spam, Captive Portal, DroneID)
- **RF Spectrum Analyzer** (2.4 GHz Wi-Fi & Zigbee real-time analyzer, Fox Hunting, Peak Tracker, Jammer & DroneID IDS, PCAP capture)
- **Software Defined Radio (SDR)** (Native 80 MS/s I/Q transceiver, CW/Packet TX, On-Chip FFT, and [ESPARGOS WebSDR](https://espargos.net/espsdr/app/) Host)
- **Surveillance Detection** (Flock Safety ALPR, Body Cams, Skimmers)
- **Zigbee** (IEEE 802.15.4 Sniffer over CLI and Spoofing)
- **Thread & Matter** (IEEE 802.15.4 / 6LoWPAN Sniffer and Broadcast)
- **GPS** (Wardriving & Geotagged Evidence Logging)

## Features
- Compatible with CatSniffer's PyCatSniffer
- Compatible with Wireshark (Live serial streaming & `.pcap` logging on microSD)
- Compatible with [ESPARGOS WebSDR](https://espargos.net/espsdr/app/) (Browser-based real-time waterfall, FFT spectrum, and I/Q capture over WebSerial)
- Real-time 2.4 GHz RF Spectrum Analyzer with Fox Hunting, 2X Peak Tracker, EMA filtering, and Smart Anti-Jammer detection
- Selectable OLED Menu Styles (`Classic`, `Icon List`, `Grid`, `Hero + Dock`)
- WarDriving (WiFi, Zigbee, Thread, BLE, Surveillance)
- Multiprotocol packet sniffing
- Supports `.pcap` and `.iq` file storage on microSD
- File Manager Web & Local OLED Browser
- Unified firmware image generation (`minino-merged.bin` and `minino-merged.hex`)
- OpenSource & Open Hardware
- OTA Updates

### TODO Features

`[] Features coming soon [x] Working features`

>[!IMPORTANT]
> OTA requires 8MB. If you want to build for other memory capacities, compile without OTA.

### Menu: Applications (`Applications`)

#### WiFi (`Applications > WiFi`)
- [x] **Wardriving**: Geotagged WiFi AP discovery with GPS coordinates.
- [x] **WiFi Sniffer / Analyzer**: Real-time 2.4 GHz channel traffic monitoring and AP summary.
- [x] **WiFi Deauther**: Injects 802.11 deauthentication frames to disconnect target devices.
- [x] **Deauth Scan (Detector)**: Continuously monitors the airwaves for active deauth attacks.
- [x] **DOS Attack (CatDoS)**: Wireless Denial of Service and beacon flooding.
- [x] **SSID Spammer**: Broadcasts thousands of fake WiFi Access Points simultaneously.
- [x] **Captive Portal**: Rogue AP with captive portal web server for testing.
- [x] **DroneID Scanner**: Listens for and decodes ASTM F3411 Remote ID drone telemetry and GPS location.
- [x] **DroneID Transmitter**: Broadcasts simulated Drone Remote ID beacons.
- [x] **Modbus TCP**: Industrial IoT and SCADA Modbus TCP endpoint scanner.
- [x] **Flock / ALPR Detector**: Detects Flock Safety ALPR cameras, body cams, and skimmers.
- [x] **Flock Simulator**: Transmits test surveillance telemetry beacons for detection auditing.
- [ ] Wireshark integration

#### BLE (`Applications > Bluetooth`)
- [x] **BLE Spammer**: Floods advertisement popups targeting iOS (Apple), Android, Windows, and Samsung devices.
- [x] **BLE HID Spoofing**: Emulates Bluetooth Human Interface Devices (keyboard, mouse, media remote).
- [x] **BLE ADV Sniffer**: Raw advertisement packet sniffer with Wireshark serial streaming.
- [x] **BLE Trackers Scanner**: Detects and tracks Apple AirTags, Samsung SmartTags, Tile, and Chipolo with RSSI signal and distance estimation.
- [x] **Wireshark integration**: Real-time BLE packet capture over serial.

#### Zigbee (`Applications > Zigbee`)
- [x] **Zigbee Sniffer**: IEEE 802.15.4 over-the-air packet sniffer.
- [x] **Zigbee Spoofing**: End Device emulation (smart light bulbs and switches).
- [x] **Wardriving**: Geotagged Zigbee network reconnaissance.
- [x] **Wireshark integration**: Zigbee frame capture streaming to Wireshark.

#### Thread (`Applications > Thread`)
- [x] **Thread Sniffer**: IEEE 802.15.4 / 6LoWPAN Thread network packet sniffer.
- [x] **Thread Broadcast**: Injects multicast and broadcast packets into Thread networks.
- [ ] Wardriving
- [x] Wireshark integration

#### Matter (`Applications > Matter`)
- [ ] Matter protocol support
- [ ] Matter CLI
- [ ] Matter Spoof

#### GPS (`Applications > GPS`)
- [x] **Wardriving**: Geotagged reconnaissance (WiFi, Zigbee, Thread) logged to SD card.
- [x] **Location**: Displays real-time latitude, longitude, and altitude coordinates.
- [x] **Speed**: Displays GPS ground speed (km/h) and heading direction.
- [x] **Date & Time**: Atomic clock synchronization and RTC update.
- [x] **Route**: Route and waypoint recording to SD card (.gpx / .csv).
- [x] **Num Sats**: Displays active satellite constellation and SNR signal quality.
- [ ] GPS Sleep

#### GPIO (`Applications > GPIO`)
- [x] **I2C Scanner**: Scans external I2C bus (SDA/SCL) device addresses.
- [x] **UART Bridge**: Hardware serial bridge / pass-through console on UART2.

#### File Manager (`Applications > File Manager`)
- [x] **File Manager Local**: On-device microSD file browser using OLED screen and buttons.
- [x] **File Manager Web**: Embedded HTTP web server for WiFi-based file downloads and uploads.

#### Spectrum (`Applications > Spectrum`)
- [x] **14-Channel Bars (`BARS`)**: Real-time 2.4 GHz bar chart overview with Peak Hold markers across all channels.
- [x] **Live Channel Scope (`SCOPE`)**: Single-channel rolling time-domain power curve and RSSI telemetry.
- [x] **2D Waterfall (`WATER`)**: Rolling spectrogram for visualizing transient transmissions and duty cycles.
- [x] **Fox Hunting Meter (`FOX`)**: High-contrast direction-finding proximity gauge (`0-100%`), calibrated dBm scale (`-90` to `-30 dBm`), Peak Hold line, and Squelch threshold marker.
- [x] **2X Peak Tracker (`PEAK`)**: Big-font display highlighting the strongest active signal (`dBm`), channel number, and center frequency (`MHz`) for rapid field readings.
- [x] **Dual-Band & Tri-Channel Sweep**: Supports both **Wi-Fi** (`Ch 1-14`, `2412-2484 MHz`) and **IEEE 802.15.4 / Zigbee** (`Ch 11-26`, `2405-2480 MHz`), plus rapid **Tri-Channel Sweep** (`Ch 1, 6, 11`).
- [x] **Smart Anti-Jammer & RF IDS**: Multi-factor evaluator (`rf_jammer_evaluate`) and EMA filter (`rf_filter_ema_rssi`) that distinguish Layer-1 RF noise/CW jammers and Layer-2 Deauth floods (`0xC0`/`0xA0`) from normal high-throughput AP traffic and adjacent-channel spillover.
- [x] **Passive DroneID Detection**: Inspects ASTM F3411 (`OUI FA:0B:BC`), French Direct Remote ID (`OUI 6A:5C:35`), and Wi-Fi Aware NAN (`88:69:19:9D:92:09`) frames to alert and log active drone UAS IDs per channel.
- [x] **Direct PCAP & CSV Logging**: Streams raw 802.11 frames to microSD (`/sdcard/spectrum_XX.pcap`) and signal telemetry to CSV without dropping the spectrum view.

#### SDR (`Applications > SDR`)
- [x] **RX I/Q Burst Capture (`[IDL]` / `[RX]`)**: Native in-process 80 MS/s hardware DMA/dump capture into reserved SRAM (`0x40840000`), live physical RSSI calculation (`dBm`), and automatic `.iq` file saving to microSD (`/sdcard/sdrcapXX.iq`).
- [x] **802.11 Raw Packet TX (`[TX:PKT]`)**: Injects bursts of custom 802.11 raw Probe Request frames (`MININO-SDR-TX`) on the selected channel (`2412-2484 MHz`, `Ch 1-14`).
- [x] **PHY Continuous Wave Carrier (`[TX:CW]`)**: Emits an unmodulated RF test carrier on the tuned frequency via the ESP32-C6 PHY synthesizer with a 30-second hardware safety timeout.
- [x] **ESPARGOS WebSDR Host (`[HOST]`)**: Streams real-time I/Q bursts (`CAP16`/`CAP20` at `80`, `40`, or `16 MS/s` with software decimation and LNA gain control `0-60 dB`) and on-chip radix-2 FFT spectrum frames (`SPEC` / `SPC1`) over USB directly to the **[ESPARGOS WebSDR App (`https://espargos.net/espsdr/app/`)](https://espargos.net/espsdr/app/)**.

---

### Menu: Settings (`Settings`)
- [x] **Display**: Configures OLED screensaver style and timeout.
- [x] **Menu Style**: Selectable OLED menu rendering views persisted in NVS (**Classic**, **Icon List**, **Grid**, and **Hero + Dock**).
- [x] **Logs Output**: Configures serial and on-screen log levels.
- [x] **SD Card Settings**: Displays SD capacity, checks format integrity, and provides FAT32 formatting.
- [x] **WiFi Settings**: Manages WiFi station credentials and radio settings.
- [x] **Stealth Mode**: Disables all LEDs, buzzer sounds, and screen animations for silent operation.
- [x] **Light Sleep Mode**: Low-power sleep mode for battery saving.
- [ ] Deep Sleep Mode

---

### Menu: About (`About`)
- [x] **Version & License**: Displays firmware version, build date, and licensing details.
- [x] **Credits & Legal**: Contributor credits and responsible disclosure notice.
- [x] **OTA Firmware Update**: Over-The-Air firmware updates via WiFi.

---

## Native 80 MS/s SDR Transceiver & ESPARGOS WebSDR Host

Minino integrates a **native, in-process Software Defined Radio (SDR)** engine (`firmware/main/modules/esp_sdr/`) running directly inside Minino OS without requiring dual-boot or a secondary `ota_1` partition. It drives the ESP32-C6 hardware I/Q dump engine at **80 MS/s**, reserves the 128 KiB SRAM Block 2 (`0x40840000 - 0x40860000`) at link time (`esp_sdr_bank.ld`), and locks the USB-Serial-JTAG reset register (`usb_uart_chip_rst_dis = 1`) while active so browser WebSerial DTR/RTS transitions never reboot the board.

### Building & Flashing the SDR Firmware Variant
Because the hardware I/Q capture bank requires 128 KiB of contiguous SRAM below `0x40840000`, the SDR variant compiles without the Zigbee and OpenThread stacks to fit within the 256 KiB lower-SRAM budget (`profiles/sdr/profile`). Every build automatically generates both individual partition binaries and unified single-file images (`minino-merged.bin` and `minino-merged.hex`):

```bash
cd firmware
make build-sdr                                    # Builds the SDR-enabled firmware in build-sdr/
idf.py -B build-sdr -p /dev/ttyACM0 flash         # Flashes bootloader, partition table, and app
```

Or flash the single unified binary at address `0x0`:
```bash
esptool.py --chip esp32c6 -p /dev/ttyACM0 write_flash 0x0 build-sdr/minino-merged.bin
```

### The 4 Operating Modes on Device (`Applications > SDR`)
Inside `Applications > SDR` (or by running `sdr_capture` in the serial console), press **`BOOT`** or **hold `RIGHT`** to cycle through the four operating modes:

| Mode Header | Function | Controls |
| :--- | :--- | :--- |
| **`MININO SDR [IDL]` / `[RX]`** | **RX I/Q Burst Capture**: Captures 8,192 32-bit I/Q words (`10-bit signed I` in bits `9:0`, `10-bit signed Q` in bits `19:10`) at 80 MS/s, computes physical RSSI (`dBm`), and saves `/sdcard/sdrcapXX.iq`. | `UP`/`DOWN`: Tune `2412-2484 MHz` (`Ch 1-14`)<br>`RIGHT`: Capture & Save<br>`BOOT` / `Hold RIGHT`: Next mode<br>`LEFT`: Exit |
| **`MININO SDR [TX:PKT]`** | **802.11 Raw Packet Injection**: Transmits 5-packet bursts of raw 802.11 Probe Request frames (`SSID: MININO-SDR-TX`) on the active channel. | `UP`/`DOWN`: Tune channel<br>`RIGHT`: Send packet burst<br>`BOOT` / `Hold RIGHT`: Next mode |
| **`MININO SDR [TX:CW]`** | **PHY Continuous Wave Tone**: Emits an unmodulated RF carrier via the ESP32-C6 PHY synthesizer (`c6_sdr_tx_carrier_start`) with an automatic **30-second safety timeout**. | `UP`/`DOWN`: Tune channel<br>`RIGHT`: Toggle CW ON/OFF<br>`BOOT` / `Hold RIGHT`: Next mode |
| **`MININO SDR [HOST]`** | **ESPARGOS WebSDR Host**: Serves the ESP-SDR line/binary protocol over USB for real-time waterfall, FFT, and I/Q streaming in the browser. | `BOOT`: Exit Host mode to RX<br>`LEFT`: Stop Host & Exit app |

### Using the SDR Host with the ESPARGOS WebSDR Page
Minino is fully compatible with the official **[ESPARGOS WebSDR application (`https://espargos.net/espsdr/app/`)](https://espargos.net/espsdr/app/)**, providing a real-time waterfall, spectrum analyzer, constellation viewer, and I/Q burst streamer in any Chromium-based browser (Chrome, Edge, Brave) via WebSerial:

1. **Connect Minino via USB** and open `Applications > SDR` on the device.
2. **Open [https://espargos.net/espsdr/app/](https://espargos.net/espsdr/app/)** in your browser and click **Connect** to select the Minino USB serial port (`/dev/ttyACM0` or `COMx`).
3. **Automatic & Manual Activation**:
   - **Auto-Switch from SDR App**: If you are anywhere inside `Applications > SDR`, as soon as the ESPARGOS web app sends its initial `SYNC` handshake, Minino **automatically switches into `[HOST]` mode**, suspends the interactive serial console, and begins streaming.
   - **Manual Toggle on OLED**: Press `BOOT` (or hold `RIGHT`) inside the SDR app until the screen displays `MININO SDR [HOST]` (`Web: espargos.net` / `App: /espsdr/app`), then click **Connect** on the web page.
   - **From Serial Console**: Type `sdr_host` in the Minino console (`115200` baud) to hand over the USB port directly.
4. **Supported ESPARGOS WebSDR Capabilities**:
   - **On-Chip Spectrum (`SPEC` / `SPC1`)**: High-FPS radix-2 Q15 FFT computed directly on the ESP32-C6 (`256`, `512`, `1024`, or `2048` bins) with DC offset modes (`DC 0`/`DC 1`) and zlib CRC32 verification.
   - **Raw I/Q Burst Streaming (`CAP16` / `CAP20`)**: 8-bit (`CAP16`) and 10-bit packed (`CAP20`) I/Q transfers with hardware-chunked 64-byte USB FIFO writes.
   - **Live Parameter Negotiation**: Supports on-the-fly center frequency tuning (`FREQ 2412..2484`), sample rate decimation (`80 MS/s`, `40 MS/s`, `16 MS/s`), and RX LNA gain control (`GAIN 0..60 dB`).
5. **Exiting Host Mode**: Disconnect from the browser (sends `RELEASE`), press `LEFT` or `BOOT` on Minino, or send `MININO\n` over serial. *(Note: Do not click "Flash Firmware..." on the ESPARGOS page, as that overwrites Minino OS with upstream esp-sdr).*

---

## RF Spectrum Analyzer & Fox Hunting

The **RF Spectrum Analyzer** (`Applications > Spectrum`, implemented in `firmware/main/apps/rf_spectrum/`) operates directly on the ESP32-C6 physical (PHY) and MAC layers to provide real-time 2.4 GHz situational awareness and transmitter hunting:

- **5 Interactive OLED Views** (cycle with `RIGHT`):
  1. **`BARS`**: 14-channel bar chart with Peak Hold indicators.
  2. **`SCOPE`**: Single-channel real-time RSSI oscilloscope trace.
  3. **`WATER`**: 2D rolling waterfall spectrogram.
  4. **`FOX` (Fox Hunting)**: High-contrast direction-finding proximity meter (`0-100%`) with calibrated dBm ticks (`-90` to `-30 dBm`), Peak Hold line (`|`), Squelch marker (`:`), and Geiger counter audio clicks (`HOLD UP`).
  5. **`PEAK` (2X Peak Tracker)**: Large 2X-scaled readout of the strongest detected signal (`dBm`), channel, and frequency (`MHz`).
- **Signal Processing & Smart Anti-Jammer**:
  - **EMA Smoothing**: Applies an Exponential Moving Average filter (`rf_filter_ema_rssi`) to stabilize noisy RSSI readings while preserving rapid peak attack.
  - **Intelligent Jammer Evaluator (`rf_jammer_evaluate`)**: Cross-checks RSSI saturation (`>= -45 dBm`), valid 802.11 frame counts, beacon presence, FCS CRC error ratios, Deauth/Disassoc counts (`0xC0`/`0xA0`), and adjacent-channel AP spillover (`+-1` channel at `>= -38 dBm`) to eliminate false positives from nearby routers.
- **Passive DroneID & PCAP Forensics**:
  - Automatically decodes ASTM F3411 / ASD-STAN (`FA:0B:BC`), French Direct Remote ID (`6A:5C:35`), and Wi-Fi Aware NAN (`88:69:19:9D:92:09`) beacons, displaying `! DRONE CH X !` and logging the drone's serial `UAS ID`.
  - Hold `DOWN` (or run `spectrum_pcap on`) to stream raw 802.11 frames into sequential `/sdcard/spectrum_00.pcap` files compatible with Wireshark.
- **Serial Console Commands**: Control and inspect live state via `spectrum_pcap [on|off|toggle]`, `spectrum_drone [clear]`, `spectrum_ids [clear]`, `threshold [<dBm>|off]`, `spectrum_audio [on|off|toggle]`, `spectrum_band [wifi|zigbee]`, and `exit_app`.

---

## Core Architecture & Resource Arbitration

Minino OS uses a deterministic resource management and lifecycle architecture across all wireless modules:
- **`radio_lease` (`components/radio_lease/`)**: Reference-counted hardware arbiter that manages mutually exclusive Wi-Fi/802.15.4 and BLE radio claims plus shared promiscuous RX leases (`RADIO_PROM_SDR_RX`, `RADIO_PROM_SPECTRUM`, `RADIO_PROM_SURV`, `RADIO_PROM_ANALYZER`).
- **`wifi_service` (`components/wifi_service/`)**: Centralizes `esp_wifi_init()` and netif creation so switching between AP, STA, Sniffer, Spammer, and SDR modes never triggers duplicate netif assertions or memory leaks.
- **`gap_dispatcher` & `gatt_dispatcher` (`components/gap_dispatcher/`)**: Multiplexes Bluedroid GAP and GATTC callbacks to multiple concurrent subscribers without callback rotation conflicts.
- **`app_manager` & Selectable OLED Menu Styles (`main/modules/menus_module/`)**: Clean `minino_app_t` (`on_enter`, `on_input`, `on_exit`) lifecycle with a dedicated FreeRTOS button input queue and four NVS-persisted UI styles (`Classic`, `Icon List`, `Grid`, `Hero + Dock`).
- **Host Unit Test Suite**: Over 20 host-side Unity and Python test suites (`make -C firmware tests`) validating protocol parsers, FFT math, channel clamping, radio leases, and NMEA/PCAP formatting on standard `gcc` without hardware.

---

## BLE Trackers Scanner
Minino includes a dedicated scanner and tracker for Bluetooth Low Energy beacon devices:
- **Supported Ecosystems**: Apple Find My / AirTags, Samsung Galaxy SmartTag / SmartTag2, Tile (Mate, Pro, Slim), and Chipolo.
- **Proximity & Signal Monitoring**: Real-time RSSI signal tracking, proximity approximation, and packet reception rates.
- **Detailed Inspection**: Live viewing of device MAC address, manufacturer payload, registered status, and last-seen timestamps.
- **Paging & Filtering**: Navigate easily across multiple active trackers in range.

## Surveillance & Flock Safety Detector
Minino features an advanced passive detection engine designed to detect physical surveillance infrastructure in real time:
- **Flock Safety / ALPR Detection**: Detects Automated License Plate Readers (ALPR) such as Flock Safety cameras, Falcon systems, and Raven audio detection units.
- **Multi-Vector Fingerprinting**:
  - **WiFi**: Matches known OUI vendor prefixes (35+ Flock OUIs, DeFlock database), surveillance SSIDs (`flocksafety`, `pigvision`, `fs ext`, `penguin`, etc.), and 802.11 Information Element (IE) signatures.
  - **BLE**: Identifies characteristic advertising beacons and vendor payloads (e.g. Xuntong BLE modules, Flock battery diagnostics).
- **Body Cameras & Skimmers**: Identifies Axon Body Worn Cameras and common Bluetooth-based payment skimmers.
- **Evidence Capture & GPS**: Automatically stores detected devices with GPS coordinates (`.csv` / `.gpx`) and captures raw network traffic (`.pcap`) on the microSD card.


Inspired by projects such as [Amini Project](https://github.com/Ocelot-Offensive-Security/Arsenal) and [USBNugget](https://github.com/HakCat-Tech/USB-Nugget).

## How to build the firmware

Check the [Developer guide](./firmware/README.md) to learn how to build the firmware from scratch.

## How to contribute <img src="https://electroniccats.com/wp-content/uploads/2018/01/fav.png" height="35"><img src="https://raw.githubusercontent.com/gist/ManulMax/2d20af60d709805c55fd784ca7cba4b9/raw/bcfeac7604f674ace63623106eb8bb8471d844a6/github.gif" height="30">

Contributions are welcome!

Please read the document [**Contribution manual**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-contribution-manual.md) which will show you how to contribute your changes to the project.

✨ Thanks to all our [Contributors](https://github.com/ElectronicCats/Minino/graphs/contributors)! ✨

See [**_Electronic Cats CLA_**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-cla.md) for more information.

See the [**Community code of conduct**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-community-code-of-conduct.md) for a vision of the community we want to build and what we expect from it.

## License

<a href="https://github.com/ElectronicCats">
    <img src="https://github.com/ElectronicCats/AjoloteBoard/raw/master/OpenSourceLicense.png" height="200" />
</a>

Electronic Cats invests time and resources providing this open source design, please support Electronic Cats and open-source hardware by purchasing products from Electronic Cats!

Designed by Electronic Cats.

Firmware released under a GNU AGPL v3.0 license. See the LICENSE file for more information.

Hardware released under a CERN Open Hardware Licence v1.2. See the LICENSE_HARDWARE file for more information.

Electronic Cats is a registered trademark, please do not use if you sell these PCBs.

Nov 29 2022
