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


# Change log

## v1.1.7.0
### Added
- SSID spam command: Now you can use custom ssid attacks
- Wardriving thread protocol
- Flash storage module to handle list of values