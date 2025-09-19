# Developer guide

You can build your own firmware using the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) framework. However, we will focus on the ESP-IDF framework in this guide.

## Table of contents

- [Developer guide](#developer-guide)
  - [Table of contents](#table-of-contents)
  - [Prerequisites](#prerequisites)
  - [Development setup](#development-setup)
  - [Building the firmware](#building-the-firmware)
    - [Flashing the firmware](#flashing-the-firmware)
    - [Monitoring the serial output](#monitoring-the-serial-output)
    - [Full build process](#full-build-process)
    - [Cleaning the project](#cleaning-the-project)
  - [Create a release](#create-a-release)
    - [Flashing release](#flashing-release)
      - [OTA](#ota)
      - [NO OTA](#no-ota)
  - [Development process](#development-process)
    - [Add new menu](#add-new-menu)
  - [WIFI](#wifi)
    - [DoS test](#dos-test)
    - [Spam SSID](#spam-ssid)
    - [Save](#save)
    - [Delete](#delete)
- [Wardriving File Formats](#wardriving-file-formats)
- [Wifi](#wifi-1)
- [Zigbee](#zigbee)
- [Thread](#thread)
- [Wardriving Thread test](#wardriving-thread-test)
- [Openthread - Install example](#openthread---install-example)
  - [ESP-IDF](#esp-idf)
    - [Get the repo](#get-the-repo)
    - [Load the example](#load-the-example)
  - [OT Border Router - Commissioner](#ot-border-router---commissioner)
  - [OT End device](#ot-end-device)
  - [View UDP Packets](#view-udp-packets)
  - [Modbus](#modbus)
  - [GATTCMD](#gattcmd)
  - [Captive Portal new version](#captive-portal-new-version)
    - [Core Features and Functionality](#core-features-and-functionality)
    - [Building a Custom Captive Portal](#building-a-custom-captive-portal)
- [Zigbee CLI](#zigbee-cli)
  - [Important](#important)
  - [Zigbee test connections](#zigbee-test-connections)
  - [Hardware Required](#hardware-required)
  - [Play with All Device Type Example](#play-with-all-device-type-example)
    - [Start Node 1 as on\_off\_light](#start-node-1-as-on_off_light)
    - [Start Node 2 as on\_off\_switch](#start-node-2-as-on_off_switch)
    - [Control the light using switch](#control-the-light-using-switch)
  - [Cool commands](#cool-commands)
- [Change log](#change-log)
  - [v1.1.7.0](#v1170)
    - [Added](#added)

## Prerequisites

> [!IMPORTANT]
> The version you need to install must be [5.3.2](https://github.com/espressif/esp-idf/releases/tag/v5.3.2); we cannot guarantee that a more recent version will compile.

- [Minino](https://electroniccats.com/store/minino/)
- [ESP-IDF v5.3.2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed
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

Find the instructions [here](menus.md).

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
Press Enter or Ctrl+C will terminate the current
command.
minino> save AP_NAME PASSWORD
```

The minino will try to connect to AP.
Once you have a AP saved if the minino app do not show the AP's exit and come back to the app to load AP, once minino found a AP available this will try to connect and if done, the next screen will show the target, if target is not configured, you need to introduce manually in the serial terminal:
```
Welcome to the Minino Console.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the current
command.
minino> web_config IP_VICTIM PORT_VICTIM _PATH_VICTIM
```
In this case our victim server are our pc so the command will be like this: `web_config 192.168.0.178 8000 /`

Then we can execute the command `catdos` to start the attack.

### Spam SSID
```bash
minino> help spam
Type 'help <command>' to get help for a specific command
  spam_delete
  spam_save
  spam_show
```

### Save
If you want to save a SSID to use for the spam you need to follow the next command:
```bash
spam_save --name=NewName "Never gonna give you up, Never gonna let you down, Never gonna run around and desert you"
```
For each `comma` we splited into a sentence so the output will showing us like:
- Never gonna give you up
- Never gonna let you down
- Never gonna run around and desert you

When you use `spam_save` and the name are the same as other saved, this will be updated.

**Limitations**:
- The name of the SSID will be written without spaces and this not exceed 13 characters

> The max limit of characters are **1024**

### Delete
Delete a SSID you need the index of this, so you can use `spam_show` and then `spam_delete idx` where `idx` are the value of the ssid you want delete.



# Wardriving File Formats
# Wifi 
- **MAC**: MAC of the Access Point
- **SSID**: Name of the Access Point
- **AuthMode**: Auth type
- **FirstSeen**: Date time
- **Channel**: Channel
- **Frequency**: Frecuency of the Ap
- **RSSI**: Signal strength
```csv
ElecCats-1.0,appRelease=1.1.6.0,model=MININO,release=1.1.6.0,device=MININO,display=SH1106 OLED,board=ESP32C6,brand=Electronic Cats,star=Sol,body=3,subBody=0
MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
84:d8:1b:9f:6f:cc,Hacknet_EXT,WPA_WPA2_PSK,2025-1-24 20:8:18,5,2432,-28,20.644129,-100.461815,1892.849976,1.500000,,,WIFI
```
# Zigbee
- **SourcePanID**: Source panID
- **SourceADDR**: Source address
- **DestinationADDR**: Destionation short address
- **Channel**: Channel
- **RSSI**: Signal strength
```csv
ElecCats-1.0,appRelease=1.1.6.0,model=MININO,release=1.1.6.0,device=MININO,display=SH1106 OLED,board=ESP32C6,brand=Electronic Cats,star=Sol,body=3,subBody=0
SourcePanID,SourceADDR,DestinationADDR,Channel,RSSI,SecurityEnabled,FrameType,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
0x7668,5a:d2:cf:46:fc:ae:75:d7,0xffff,16,-25,No,Data,4.130518,4.130518,1907.400024,1.500000,,,Zigbee
```
# Thread
- **DestinationPAN**: Destination from the source
- **Destination**: Destination short address
- **ExtendedSource**: Extended source address from the source
- **Channel**: Channel of the capture
- **UDPSource**: UDP Port where source send the message
- **UDPDestination**: UDP port where destination receive
- **Protocol**: Thread or IEEE 802.15.4 message type
```csv
ElecCats-1.0,appRelease=1.1.6.0,model=MININO,release=1.1.6.0,device=MININO,display=SH1106 OLED,board=ESP32C6,brand=Electronic Cats,star=Sol,body=3,subBody=0
DestinationPAN,Destination,ExtendedSource,Channel,UDPSource,UDPDestination,Protocol,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
0xb24b,0xffff,ae:27:c5:e6:1e:d7:3c:e3,11,19788,19788,MLE,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xffff,a2:e7:b5:59:69:77:cc:ed,11,19788,19788,MLE,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xc000,01:00:00:5a:30:0d:f4:00,11,,,IEEE 802.15.4,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xf400,01:00:00:b5:ef:0d:c0:00,11,,,IEEE 802.15.4,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xc000,01:00:00:5a:31:0d:f4:00,11,,,IEEE 802.15.4,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xffff,a2:e7:b5:59:69:77:cc:ed,11,19788,19788,MLE,4.130068,4.130068,0.000000,1.500000,,,Thread
0xb24b,0xffff,ae:27:c5:e6:1e:d7:3c:e3,11,19788,19788,MLE,4.130068,4.130068,0.000000,1.500000,,,Thread
```

# Wardriving Thread test
# Openthread - Install example
## ESP-IDF
### Get the repo
```shell
git clone https://github.com/espressif/esp-idf
cd esp-idf
git submodule update --init --recursive
./install.sh
./export.sh
```

### Load the example
```shell
cd examples/openthread/ot_cli
idf.py set-target esp32h2
idf.py -p {PORT} erase-flash flash monitor
```

> Some configuration with the developer board can not working with the USB connection, then use the serial interface

## OT Border Router - Commissioner

```shell
> dataset init new

Done
> dataset

Active Timestamp: 1
Channel: 13
Channel Mask: 0x07fff800
Ext PAN ID: 5313f58f402c1819
Mesh Local Prefix: fd96:26d8:408f:b50e::/64
Network Key: 15766f59ec0a271e69bcdf5343e185af
Network Name: OpenThread-2102
PAN ID: 0x2102
PSKc: 004e5d0799d448f8ec7b30eafab082e7
Security Policy: 672 onrc 0
Done
> dataset commit active

Done
> ifconfig up

Done
I (350303) OT_STATE: netif up
> thread start

I(361183) OPENTHREAD:[N] Mle-----------: Role disabled -> detached
Done
> I(361643) OPENTHREAD:[N] Mle-----------: Attach attempt 1, AnyPartition reattaching with Active Dataset
I(368243) OPENTHREAD:[N] RouterTable---: Allocate router id 12
I(368243) OPENTHREAD:[N] Mle-----------: RLOC16 fffe -> 3000
I(368243) OPENTHREAD:[N] Mle-----------: Role detached -> leader
I(368263) OPENTHREAD:[N] Mle-----------: Partition ID 0x52de31e6
# After a moment, check the device state, when the state is 'Leader' then the network it is ready
> state
leader
Done
>
```

**Commands**:
- `dataset init new`: Create a new network
- `dataset`: Show the network details
- `dataset commit active`: Setting up the configuration and set as active
- `ifconfig up`: Activate the phy interface
- `thread start`: Activate the thread service
- `state`: Show the device status

## OT End device
```shell
> dataset networkkey 15766f59ec0a271e69bcdf5343e185af

Done
> dataset commit active

Done
> ifconfig up

Done
I (2479103) OT_STATE: netif up
> thread start

I(2484933) OPENTHREAD:[N] Mle-----------: Role disabled -> detached
Done
> I(2485463) OPENTHREAD:[N] Mle-----------: Attach attempt 1, AnyPartition reattaching with Active Dataset
I(2492073) OPENTHREAD:[N] Mle-----------: Attach attempt 1 unsuccessful, will try again in 0.284 seconds
I(2492373) OPENTHREAD:[N] Mle-----------: Attach attempt 2, AnyPartition
I(2494743) OPENTHREAD:[N] Mle-----------: Delay processing Announce - channel 13, panid 0x2102
I(2495013) OPENTHREAD:[N] Mle-----------: Processing Announce - channel 13, panid 0x2102
I(2495013) OPENTHREAD:[N] Mle-----------: Role detached -> disabled
I(2495023) OPENTHREAD:[N] Mle-----------: Role disabled -> detached
I(2495493) OPENTHREAD:[N] Mle-----------: Attach attempt 1, AnyPartition
I(2496323) OPENTHREAD:[N] Mle-----------: RLOC16 fffe -> 3001
I(2496323) OPENTHREAD:[N] Mle-----------: Role detached -> child
> state

child
Done
```

**Commands**:
- `dataset networkkey`: Set the networkkey with the Border Route entwork
- `dataset commit active`: Setting up the configuration and set as active
- `ifconfig up`: Activate the phy interface
- `thread start`: Activate the thread service
- `state`: Show the device status

## View UDP Packets
In the OT Border Router:
```shell
> udp open

Done
> udp bind :: 20617

Done
> ipaddr

fd96:26d8:408f:b50e:0:ff:fe00:fc00       # Routing Locator (RLOC)
fd96:26d8:408f:b50e:0:ff:fe00:3000
fd96:26d8:408f:b50e:4bff:b763:4835:9104  # Mesh-Local EID (ML-EID)
fe80:0:0:0:28ef:a315:c3a7:f8a0           # Link-Local Address (LLA)
Done
```

In the End device:
```shell
udp open
udp send fd96:26d8:408f:b50e:4bff:b763:4835:9104 20617 CatsRules
```

The Border Router recived:
```shell
> 9 bytes from fd96:26d8:408f:b50e:bca0:8fb6:ed5a:4a27 49154 CatsRules
```


## Modbus

> ⚠️ Warning: These commands are based on my specific setup. Your configuration may differ, so make sure to adjust the IP address, request type, registers, and coils according to your environment.

> **Important**: The registers and coils depend on your setup — do not assume mine will work for you.

```bash
# List the availables Access Point
list
# Connect to your Access Point
connect 0
# Set the server IP and Port 
mb_engine_set_server 192.168.0.2500
# Set the request type
mb_engine_set_req registers 1 2 1234
# Connect to the server
mb_engine_connect
# Send single attack
mb_engine_send_req
# Send loop attack of writer
mb_engine_start_writer
# Send dos attack
mb_engine_start_dos
# Stop any attack
mb_engine_stop_attack
```

> For more information you can use the `help` function and the command to view more information about it.


## GATTCMD
A command line for interect with Bluetooth Characteristics
- `gattcmd_scan`: Scan for MAC Address
- `gattcmd_enum`: Enum the Characteristics from the device
- `gattcmd_write`: Write a value in a GATT characteristic

> This app is still under development, all the testing have been done with devices with no encription for gatt writing.

## Captive Portal new version
The updated captive portal module enables the Minino device to function as a Wi-Fi Access Point (AP), allowing it to intercept client connections and present a fully customizable web interface. This update brings enhanced flexibility, user data handling, and configuration options, making it easier to create engaging and effective captive experiences.

### Core Features and Functionality
- **Custom Portals from SD Card**
You can now store your own HTML-based portals on a microSD card and load them at runtime. This allows for dynamic customization of the captive portal without needing to recompile the firmware.
- **User Input Logging (Optional Dump to SD)**
Captured user input from the portal (such as credentials or form data) can be saved to a `.txt` file on the SD card. This behavior can be enabled or disabled from a new Preferences menu in the device interface.
- **Extended Input Support**
The portal now supports multiple form input fields. Specifically, you can use up to four predefined input names: user1, user2, user3, and user4. These are mapped to typical use cases like email, password, or custom fields.
- **Access Point Modes: Standalone and Replicate**
You can now choose between two operational modes:
  1. Standalone: Minino creates a Wi-Fi AP using a user-defined SSID.
  2. Replicate: Minino scans for nearby Wi-Fi networks and clones the SSID of a selected access point (useful for social engineering or penetration testing scenarios).
- **Custom AP Configuration via Menuconfig**
The AP name, password, IP configuration, and more can now be set directly through the menuconfig interface—no need to modify the source code.

### Building a Custom Captive Portal
To create your own portal, use the provided root.html template as a reference. This HTML page includes a form that sends a `GET` request to the `/validate` endpoint:
``` html
<form action="/validate" method="get" class="row" style="padding: 1rem;">
  <label for="user1" style="margin-top: 1rem;">Email</label>
  <input type="email" name="user1" class="form-control">

  <label for="user2" style="margin-top: 1rem;">Password</label>
  <input type="password" name="user2" class="form-control">

  <label for="user3" style="margin-top: 1rem;">Custom Field</label>
  <input type="text" name="user3" class="form-control">

  <label for="user4" style="margin-top: 1rem;">Custom Field</label>
  <input type="text" name="user4" class="form-control">

  <button type="submit" class="btn" style="margin-top: 1rem;">Send</button>
</form>
```
The form inputs should use the reserved names `user1` through `user4` to ensure the system captures the data correctly.

The value: `idf.py menuconfig > Component config > HTTP Server > (1024)Max HTTP Request Header Length` will set to 1024 to avoid issues with Android

# Zigbee CLI

## Important
**To use the CLI you need to enable it in the `Settings -> ZB CLI` to use, if you want to use zigbee apps you need to disable the CLI before use the app like the sniffer.**

## Zigbee test connections
This test code shows how to configure Zigbee device and use it as HA Device Types, such as On/Off Switch, Window Covering, Door Lock, On/Off Light Device and so on.

## Hardware Required
* Minino
* Another development board with ESP32-H2 SoC as communication peer (loaded with esp_zigbee_all_device_types_app example or other examples).

## Play with All Device Type Example
This section demonstrate the process to create HA on_off_light and on_off_switch devices.

### Start Node 1 as on_off_light

Create and register the HA standard on_off_light data model:

```bash
esp> zha add 1 on_off_light
I (518033) cli_cmd_zha: on_off_light created with endpoint_id 1
esp> dm register
esp>
```

Form the network and open the network for joining:

```bash
esp> bdb_comm start form
I (839703) cli_cmd_bdb: ZDO signal: ZDO Config Ready (0x17), status: ESP_FAIL
I (839703) cli_cmd_bdb: Zigbee stack initialized
W (842093) cli_cmd_bdb: Network(0x79d0) closed, devices joining not allowed.
I (842103) cli_cmd_bdb: Formed network successfully (Extended PAN ID: 0x744dbdfffe602dfd, PAN ID: 0x79d0, Channel:11, Short Address: <SHORT_ADDRESS>)
esp> network open -t 200
I (860763) cli_cmd_bdb: Network(0x79d0) is open for 200 seconds
```

### Start Node 2 as on_off_switch

Create and register the HA standard on_off_switch data model:

```bash
esp> zha add 2 on_off_switch
I (914051) cli_cmd_zha: on_off_switch created with endpoint_id 2
esp> dm register
esp>
```

Joining the network:

```bash
esp> bdb_comm start steer
I (930721) cli_cmd_bdb: ZDO signal: ZDO Config Ready (0x17), status: ESP_FAIL
I (930721) cli_cmd_bdb: Zigbee stack initialized
W (933611) cli_cmd_bdb: Network(0x79d0) closed, devices joining not allowed.
I (933981) cli_cmd_bdb: Network(0x79d0) is open for 180 seconds
I (933991) cli_cmd_bdb: Joined network successfully (Extended PAN ID: 0x744dbdfffe602dfd, PAN ID: 0x79d0, Channel:11, Short Address: 0x0935)
```

### Control the light using switch

Get the light on/off status by reading the attribute from the switch:

zcl send_gen read -d 0xe643 --dst-ep 1 -e 2 -c 6 -a 0


```bash
esp> zcl send_gen read -d <SHORT_ADDRESS> --dst-ep 1 -e 2 -c 6 -a 0
I (1347151) cli_cmd_zcl: Read response: endpoint(2), cluster(0x06)
I (1347161) cli_cmd_zcl: attribute(0x00), type(0x10), data size(1)
I (1347171) : 0x40818c02   00                                                |.|
```

Send "On" command from the switch:

```bash
esp> zcl send_raw -d <SHORT_ADDRESS> --dst-ep 1 -e 2 -c 6 --cmd 0x01
W (1405221) ESP_ZB_CONSOLE_APP: Receive Zigbee action(0x1005) callback
```

Check the light status again:

```bash
esp> zcl send_gen read -d <SHORT_ADDRESS> --dst-ep 1 -e 2 -c 6 -a 0
I (1420021) cli_cmd_zcl: Read response: endpoint(2), cluster(0x06)
I (1420021) cli_cmd_zcl: attribute(0x00), type(0x10), data size(1)
I (1420031) : 0x40818ca6   01                                                |.|
```
## Cool commands
- **Netowrk discovery**: `network scan -m 0x07fff800`
- **Network energy detection**: `network ed_scan -m 0x07fff800`
- **Discover Attributes**: `zcl send_gen disc_attr -d 0xe643 --dst-ep 1 -e 2 -c 4`
- **Request Active End Point**: `zdo request active_ep -d 0xe643`
- **Request Neighbors**: `zdo request neighbors -d 0xe643`
zdo request neighbors -d 0x0000

# Change log

## v1.1.7.0
### Added
- SSID spam command: Now you can use custom ssid attacks
- Wardriving thread protocol
- Flash storage module to handle list of values
