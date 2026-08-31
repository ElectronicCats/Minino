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

Minino is an original multiprotocol, and multiband board made for sniffing, communicating, and attacking IoT (Internet of Things) devices. It was designed as a mini Cat that integrates the powerful ESP32C6 and a GPS, microSD with OLED.  This board is a mini Swiss army knife for IoT security researchers, developers, and enthusiasts. 

Minino can operate in 7 different technologies:
- BLE (Trackers scanner for AirTag, SmartTag, Tile & BLE SPAM)
- Wi-Fi (Sniffer, deauthenticator, and attacks)
- Surveillance Detection (Flock Safety ALPR, Body Cams, Skimmers)
- Zigbee (Sniffer over CLI and Spoofing)
- Thread
- Matter
- GPS (WarDriving & Evidence Logging)

## Features
- Compatible with Pycatsniffer of Catsniffer
- Compatible with Wireshark
- WarDriving (WiFi, Zigbee, Thread, BLE, Surveillance)
- Sniffing multiprotocol
- Support file .pcap in microSD
- File Manager Web
- OpenSource
- Open Hardware
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

---

### Menu: Settings (`Settings`)
- [x] **Display**: Configures OLED screensaver style and timeout.
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

Firmware released under an GNU AGPL v3.0 license. See the LICENSE file for more information.

Hardware released under an CERN Open Hardware Licence v1.2. See the LICENSE_HARDWARE file for more information.

Electronic Cats is a registered trademark, please do not use if you sell these PCBs.

Nov 29 2022
