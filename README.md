# Minino

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

## Table of contents

- [Features](#features)
    - [Future Features](#future-features)
- [Getting started](#getting-started)
  - [UI overview](#ui-overview)
  - [User guide](#user-guide)
  - [Getting updates](#getting-updates)
    - [OTA update](#ota-update)
    - [Manual update](#manual-update)
  - [Developer guide](#developer-guide)
- [How to contribute](#how-to-contribute)
- [License](#license)

## Features
- Compatible with Pycatsniffer of Catsniffer
- Compatible with Wireshark
- WarDriving
- Sniffing multiprotocol
- Support file .pcap in microSD
- OpenSource
- Open Hardware
- OTA Updates

### Future Features
- Thread Broadcast
- Zigbee Sniffer and deauthenticator
- Matter CLI
- Save files in SD

Inspired by projects such as [Amini Project](https://github.com/Ocelot-Offensive-Security/Arsenal) and [USBNugget](https://github.com/HakCat-Tech/USB-Nugget).

## Getting started

### UI overview

Minino has a simple UI based on a 128x64 OLED display and 4 buttons. With the UP and DOWN buttons you can navigate through the options, with the LEFT you can go back and with the RIGHT you can select an option.

![UI](./examples/minino/resources/ui_overview.gif)

> There are specific applications that doesn't follow the UI menu flow, just interact with them to see what they do.

### User guide

Your Minino comes with a pre-installed firmware, just add the batteries or connect it to a USB power source and turn it on.

> If Minino doesn't turn on, press the `RESET` button.

See the [Wiki](https://github.com/ElectronicCats/Minino/wiki) for more information about the applications and how to use them.

### Getting updates

First check the current version of your firmware by going to `About > Version` in your Minino, then check the latest version in the [releases](https://github.com/ElectronicCats/Minino/releases). If there is a new version, continue with the steps below.

- [OTA update](#ota-update)
- [Manual update](#manual-update)

#### OTA update

TODO: Add OTA update instructions.

#### Manual update

#### Requirements

- [esptool.py](https://pypi.org/project/esptool/)

To update your Minino firmware, follow these steps:

1. Download the latest firmware from the [releases](https://github.com/ElectronicCats/Minino/releases), make sure to download the one that has the `build_files.zip` file.

2. Extract the `build_files.zip` file.

3. Open a terminal and navigate to the extracted folder. Make sure you see the following files:

```
bootloader/
minino.bin
partition_table/
```

4. Connect your Minino to your computer using a USB cable and check the port it is connected to.

> On Windows, you can check it in the Device Manager, on Linux you can check it with the `ls /dev/ttyUSB*` or `ls /dev/ttyACM*` command, and on macOS you can check it with the `ls /dev/cu.usb*` command.

5. Run the following command to flash the firmware:

```bash
esptool.py --chip esp32c6 -p $PORT -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 bootloader/bootloader.bin 0x10000 minino.bin 0x8000 partition_table/partition-table.bin
```

> Replace `$PORT` with the port your Minino is connected to.

### Developer guide

You can build your own firmware using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) framework, just follow the instructions in this [link](/examples/minino/README.md).

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
