# Minino

![GitHub release (with filter)](https://img.shields.io/github/v/release/ElectronicCats/Minino?color=%23008000)
![GitHub actions](https://img.shields.io/github/actions/workflow/status/ElectronicCats/Minino/pre-commit.yml)
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

Minino can operate in 6 different technologies:
- BLE (Airtags scanner and SPAM)
- Wi-Fi (Sniffer and deauthenticator)
- Zigbee (Sniffer over CLI and Spoofing)
- Thread
- Matter
- GPS (WarDriving)

## Features
- Compatible with Pycatsniffer of Catsniffer
- Compatible with Wireshark
- WarDriving
- Sniffing multiprotocol
- Support file .pcap in microSD
- File Manager Web
- OpenSource
- Open Hardware
- OTA Updates

### TODO Features

`[] Features coming soon [x] Working features`

### WIFI
- [x] Wardriving
- [x] WiFi sniffer
- [x] WiFi deauther
- [x] DOS Attack (Control with Console and Minino)
- [x] Analizer
- [ ] Wireshark integration
### BLE
- [ ] BLE Sniffer
- [x] BLE Spammer
- [ ] BLE Spoffing
- [x] BLE Trackers Scanner (AirTags, Tile, etc)
- [ ] Wireshark integration
### Zigbee
- [x] Zigbee sniffer
- [x] Zigbee spoofing (Switch End Device)
- [ ] Wardriving
- [x] Wireshark integration

### Thread
- [x] Thread sniffer
- [x] Thread broadcast
- [ ] Wardriving
- [x] Wireshark integration

### Matter
- [ ] Matter protocol support
- [ ] Matter CLI

### Tools
- [x] OTA Firmware Update
- [x] GPS Location
- [x] GPS Speed
- [x] GPS Time
- [x] SD
- [x] File Manager Web (Local AP and WIFI access)
- [ ] File Manager Local
- [ ] I2C Scanner
- [ ] UART2

### Settings
- [x] Change screensaver
- [x] Change time screensaver

Inspired by projects such as [Amini Project](https://github.com/Ocelot-Offensive-Security/Arsenal) and [USBNugget](https://github.com/HakCat-Tech/USB-Nugget).

## How to contribute <img src="https://electroniccats.com/wp-content/uploads/2018/01/fav.png" height="35"><img src="https://raw.githubusercontent.com/gist/ManulMax/2d20af60d709805c55fd784ca7cba4b9/raw/bcfeac7604f674ace63623106eb8bb8471d844a6/github.gif" height="30">

Contributions are welcome!

Please read the document [**Contribution Manual**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-contribution-manual.md) which will show you how to contribute your changes to the project.

✨ Thanks to all our [contributors](https://github.com/ElectronicCats/Minino/graphs/contributors)! ✨

See [**_Electronic Cats CLA_**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-cla.md) for more information.

See the [**community code of conduct**](https://github.com/ElectronicCats/electroniccats-cla/blob/main/electroniccats-community-code-of-conduct.md) for a vision of the community we want to build and what we expect from it.

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
