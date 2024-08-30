# Developer guide

You can build your own firmware using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) framework or [Arduino](https://www.arduino.cc/en/software). However, we will focus on the ESP-IDF framework in this guide.

## Table of contents

- [Prerequisites](#prerequisites)
- [Development setup](#development-setup)
  - [Building the firmware](#building-the-firmware)
  - [Flashing the firmware](#flashing-the-firmware)
  - [Monitoring the serial output](#monitoring-the-serial-output)
  - [Full build process](#full-build-process)
  - [Cleaning the project](#cleaning-the-project)
- [Create a release](#create-a-release)
- [Development process](#development-process)
  - [Add a new menu](#add-a-new-menu)
    - [Menus structure](#menus-structure)
    - [Steps to add a new menu](#steps-to-add-a-new-menu)

## Prerequisites

- [Minino](https://electroniccats.com/store/minino/)
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed
- [Make](https://www.gnu.org/software/make/) installed
- [pre-commit](https://pre-commit.com/) installed (optional but recommended)

## Development setup

1. Clone this repository:

Using HTTPS:

```bash
git clone https://github.com/ElectronicCats/Minino.git
```

Using SSH:

```bash
git clone git@github.com:ElectronicCats/Minino.git
```

2. Change to the firmware directory:

```bash
cd Minino/examples/minino
```

3. Set the IDF_PATH environment variable:

```bash
get_idf
```

> **Note:** `get_idf` should be an alias you created if you followed the [ESP-IDF installation guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

4. Setup the project environment:

```bash
make setup
```

## Building the firmware

To build the firmware, run the following command:

```bash
make compile
```

### Flashing the firmware

1. Connect your Minino to your computer and identify the port it is connected to. For example, `/dev/ttyUSB0` or `/dev/ttyACM0` on Linux, `COM1` or `COM2` on Windows, or `/dev/cu.usbmodem14401` on macOS.

2. Set the PORT to an environment variable, for example:

```bash
export PORT=/dev/ttyUSB0
```

> Replace `/dev/ttyUSB0` with the port your Minino is connected to.

3. Flash the firmware:

```bash
make flash
```

### Monitoring the serial output

To monitor the serial output, run the following command:

```bash
make monitor
```

### Full build process

To do all the previous steps in one command, run:

```bash
make all
```

### Cleaning the project

If you you have dependencies issues or want to clean the project, run the following command:

```bash
make clean
```

## Create a release

To create a release, you need to update the firmware version in the `menuconfig`. To do this, run:

```bash
idf.py menuconfig
```

Navigate to `General project` -> `Firmware version` and update the version number.

Get the build files:

```bash
./get_build.sh
```

If you can't run the script, make sure it has the correct permissions:

```bash
chmod +x get_build.sh
```

> **Note:** On Windows, you can run the script using Git Bash.

The build files will be in the `build_files.zip` file. Now you can create a release on GitHub and attach the `build_files.zip` file.


# Flashing release
## OTA
### Table for [ESP Tool](https://espressif.github.io/esptool-js/)
| Flash Address | File                 |
|---------------|----------------------|
| 0x0           | bootloader.bin       |
| 0x8000        | partition-table.bin  |
| 0x15000       | ota_data_initial.bin |
| 0xa0000       | minino.bin           |

### Command

```bash
 python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x15000 ota_data_initial.bin 0xa0000 minino.bin
```
## NO OTA
| Flash Address | File                 |
|---------------|----------------------|
| 0x0           | bootloader.bin       |
| 0x8000        | partition-table.bin  |
| 0x20000       | minino.bin           |
```bash
 python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x20000 minino.bin
```