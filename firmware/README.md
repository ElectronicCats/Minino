# Developer guide

You can build your own firmware using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) framework. However, we will focus on the ESP-IDF framework in this guide.

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

> [!IMPORTANT]
> The version you need to install must be [5.3.1](https://github.com/espressif/esp-idf/releases/tag/v5.3.1); we cannot guarantee that a more recent version will compile.

- [Minino](https://electroniccats.com/store/minino/)
- [ESP-IDF v5.3.1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed
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
cd Minino/firmware
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


### Flashing release

#### OTA

**Table for [ESP Tool](https://espressif.github.io/esptool-js/)**
| Flash Address | File                 |
|---------------|----------------------|
| 0x0           | bootloader.bin       |
| 0x8000        | partition-table.bin  |
| 0x15000       | ota_data_initial.bin |
| 0xa0000       | minino.bin           |

**Command**

```bash
 python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x15000 ota_data_initial.bin 0xa0000 minino.bin
```
#### NO OTA
| Flash Address | File                 |
|---------------|----------------------|
| 0x0           | bootloader.bin       |
| 0x8000        | partition-table.bin  |
| 0x20000       | minino.bin           |
```bash
 python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x20000 minino.bin
```

## Development process

### Add new menu

#### Menus structure

TODO

#### Steps to add new menu

TODO

### Add a component

To add a new component, follow these steps:

1. Create a component under the `components` directory using the following command:

```bash
idf.py create-component -C components/ <component_name>
```

2. Add the component to the `CMakeLists.txt` file in the `main` directory:

```cmake
set(EXTRA_COMPONENT_DIRS components/<component_name>)
```

Your component is now ready to be used in the project.

### Add configuration to enable/disable ESP_LOGs of a specific component/module

1. Add a new configuration option in the `Kconfig.projbuild` file in the `minino_config` component. For example, for the `uart_bridge` component, add the following:

```cmake
config UART_BRIDGE_DEBUG
			bool "UART Bridge debug"
			default false
			help
				Enable UART Bridge debug.
```

> [!IMPORTANT]
> Add the configuration using alphabetical order.

> Note: Make sure to include the configuration under the right section, for now, there are two sections: `"Modules debug configuration"` `"Components debug configuration"`.

2. Make sure your configuration is visible in the `menuconfig`, run the following command and go to `General project` -> `Debug configuration`:

```bash
idf.py menuconfig
```

3. Add the following instruction to the `begin` function or the function where your component/module is initialized:

```c
#if !defined(CONFIG_MODULE_NAME_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
```

For example, for the `uart_bridge` component, add the following to the `uart_bridge.c` file:

```c
static const char* TAG = "uart_bridge";

esp_err_t uart_bridge_begin(int baud_rate, int buffer_size) {
#if !defined(CONFIG_UART_BRIDGE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  ...
}
```

4. You can now enable/disable the logs of the component/module using the `menuconfig`.

## WIFI

### DoS test
First run a python simple server with:
`python3 -m http.server`

Next open the DoS app in minino and if you haven't a AP saved in the serial terminal you need to add your AP:
```
Welcome to the Minino Console.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the console environment.
minino> save AP_NAME PASSWORD
```

The minino will try to connect to AP.
Once you have a AP saved if the minino app do not show the AP's exit and come back to the app to load AP, once minino found a AP available this will try to connect and if done, the next screen will show the target, if target is not configured, you need to introduce manually in the serial terminal:
```
Welcome to the Minino Console.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the console environment.
minino> web_config IP_VICTIM PORT_VICTIM _PATH_VICTIM
```
In this case our victim server are our pc so the command will be like this: `web_config 192.168.0.178 8000 /`

Then we can execute the command `catdos` to start the attack.