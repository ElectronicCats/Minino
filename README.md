# Minino

Minino is based on the [ESP32-C6](https://www.espressif.com/en/products/socs/esp32-c6) microcontroller, it combines WiFi, Bluetooth Low Energy (BLE), Zigbee and Thread protocols to create a powerful tool for security researchers and pentesters.

## Applications

Current working applications include:

- [x] WiFi sniffer
- [x] WiFi deauther
- [x] BLE spammer
- [x] BLE trackers scanner (AirTags, Tile, etc)
- [x] Zigbee sniffer
- [x] Zigbee spoofing (Switch End Device)
- [x] Thread broadcast
- [x] GPS tracker

Coming soon:

- [ ] Thread sniffer
- [ ] Wireshark integration
- [ ] Wardriving
- [ ] Matter protocol support

## UI overview

Minino has a simple UI based on a 128x64 OLED display and 4 buttons. With the UP and DOWN buttons you can navigate through the options, with the LEFT you can go back and with the RIGHT you can select an option.

![UI](./examples/ESP-IDF/minino/src/ui_overview.gif)

> There are specific applications that doesn't follow the UI menu flow, just interact with them to see what they do.

## Getting started

### User guide

Your Minino comes with a pre-installed firmware, just add the batteries or connect it to a USB power source and turn it on.

See the [Wiki]() for more information about the applications and how to use them.

### Getting updates

First check the current version of your firmware by going to `About > Version` in your Minino, then check the latest version in the [releases](). If there is a new version, continue with the steps below.

To update your Minino firmware, follow these steps:

TODO: Add instructions

### Developer guide

You can build your own firmware using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) framework, just follow the instructions in this [link]().

## Thanks

Based in the project [Amini Project](https://github.com/Ocelot-Offensive-Security/Arsenal)
Based in the project [USBNugget](https://github.com/HakCat-Tech/USB-Nugget)

## License

![OpenSourceLicense](https://github.com/ElectronicCats/AjoloteBoard/raw/master/OpenSourceLicense.png)

Electronic Cats invests time and resources providing this open source design, please support Electronic Cats and open-source hardware by purchasing products from Electronic Cats!

Designed by Electronic Cats.

Firmware released under an GNU AGPL v3.0 license. See the LICENSE file for more information.

Hardware released under an CERN Open Hardware Licence v1.2. See the LICENSE_HARDWARE file for more information.

Electronic Cats is a registered trademark, please do not use if you sell these PCBs.

Nov 29 2022
