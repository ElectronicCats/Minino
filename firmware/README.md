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
    - [Board profiles](#board-profiles)
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
  - [BLE](#ble)
    - [Spam](#spam)
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
  - [Unreleased](#unreleased)
  - [v1.1.16.1](#v11161)
  - [v1.1.16.0](#v11160)
  - [v1.1.15.2](#v11152)
  - [v1.1.15.1](#v11151)
  - [v1.1.15.0](#v11150)
  - [v1.1.14.1](#v11141)
  - [v1.1.14.0-beta](#v11140-beta)
  - [v1.1.13.0](#v11130)
  - [v1.1.12.0](#v11120)
  - [v1.1.11.0](#v11110)
  - [v1.1.10.0](#v11100)
  - [v1.1.9.0](#v1190)
  - [v1.1.8.0](#v1180)
  - [v1.1.7.2](#v1172)
  - [v1.1.7.1](#v1171)
  - [v1.1.7.0](#v1170)

## Prerequisites

> [!IMPORTANT]
> The version you need to install must be [5.5.1](https://github.com/espressif/esp-idf/releases/tag/v5.5.1); we cannot guarantee that a more recent version will compile.

- [Minino](https://electroniccats.com/store/minino/)
- [ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/get-started/index.html) installed
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

To build the standard firmware, run the following command:

```bash
make compile
```

The firmware includes **SDR** (`Applications > SDR`, `sdr_capture`,
`sdr_host`), Spectrum, BLE, Wi-Fi, Zigbee and Thread in one image.

The SDR engine needs the 128 KiB SRAM block at `0x40840000` as its capture
bank. That block only leaves the heap on an **SDR boot**: opening the SDR app
(or running `sdr_host`) reboots the board with the bank reserved and opens the
app, and leaving it reboots back to a normal boot, landing in
`Applications`. The other apps keep the whole heap. A crash or a power cycle
in SDR mode comes back as a normal boot.

The default profile includes Zigbee/OpenThread, so `zbmesh`, `warbee`,
`warthread`, `zigbee_sniffer`, `thread_sniffer` and `broadcast` are native
entries in the menu.

If `firmware/sdkconfig` comes from an older checkout where the stacks or SDR
were disabled, `make compile` rewrites those symbols to `y` automatically so
the apps are not silently dropped from the build.

In the combined build, `bt_spam` refuses to start below 18 KB free and stops
below 8 KB during a run. That prevents the controller advertising failure when
Thread Sniffer/Broadcast has left the heap low.

To run the host-side unit test suites (the 17 Unity C suites plus the Python
flash-tooling test), run:

```bash
make tests
```

> **Note:** Every build automatically generates unified single-file firmware images (`minino-merged.bin` and `minino-merged.hex`) inside the build directory (`build/` or `build-<profile>/`), ready to be flashed at offset `0x0`.

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

### Board profiles

`make compile` builds the default profile into `build/` using `./sdkconfig`. To
cross-build the per-event board variants, run:

```bash
make builds
```

Each board overlays its own config (`profiles/<board>/profile`) and lands in
`build-<board>/`. The profiles are `minino`, `bsides`, `dragonjar`, `ekoparty`,
`bugcon` and `bsseattle`. Build a single one with
`idf.py @profiles/<board>/profile build`, and verify every image still fits its
partition table with `make flash_fit`.

### Cleaning the project

If you have dependency issues or want to clean the project, run the following command:

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
| 0x440000      | minino.bin           |
```bash
 python -m esptool --chip esp32c6 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0x440000 minino.bin
```

## Development process

### Add new menu

Find the instructions [here](menus.md).

## WIFI

### DoS test
First, run a simple Python HTTP server with:
`python3 -m http.server`

Next, open the DoS app in Minino. If you do not have an AP saved, add your AP in the serial terminal:
```
Welcome to the Minino Console.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the current
command.
minino> save AP_NAME PASSWORD
```

Minino will try to connect to the AP.
Once you have an AP saved, if the Minino app does not show the APs, exit and re-enter the app to reload the list. Once Minino finds an available AP, it will connect and display the target screen. If a target is not configured, set it manually in the serial terminal:
```
Welcome to the Minino Console.
Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
Press Enter or Ctrl+C will terminate the current
command.
minino> web_config IP_VICTIM PORT_VICTIM _PATH_VICTIM
```
In this case, our target server is our PC, so the command looks like this: `web_config 192.168.0.178 8000 /`

Then execute the command `catdos` to start the attack.

### Spam SSID
```bash
minino> help spam
Type 'help <command>' to get help for a specific command
  spam_delete
  spam_save
  spam_show
```

### Save
To save an SSID list for spamming, run the following command:
```bash
spam_save --name=NewName "Never gonna give you up, Never gonna let you down, Never gonna run around and desert you"
```
Each `comma` splits the string into a separate SSID entry, producing:
- Never gonna give you up
- Never gonna let you down
- Never gonna run around and desert you

When you use `spam_save` with an existing list name, the entry is updated.

**Limitations**:
- The list name must be written without spaces and must not exceed 13 characters.

> The maximum character limit is **1024**.

### Delete
To delete a saved SSID list, run `spam_show` to find its index, then run `spam_delete idx` where `idx` is the index of the SSID list you want to delete.

## BLE

### Spam

The BLE Spam app floods nearby devices with crafted advertising packets. Open
it from `Applications > Bluetooth > Spam` and pick a target mode:

| Mode | Launch command | What it advertises |
|----------|----------------------|-------------------------------------------------|
| Apple | `launch spam_apple` | Apple proximity-pairing popups |
| Android | `launch spam_android` | Android Fast Pair popups |
| Windows | `launch spam_windows` | Windows Swift Pair popups |
| Samsung | `launch spam_samsung` | Samsung pairing popups |
| Saturate | `launch spam_saturate` | **Lab-only** channel-saturation stress test |
| Matter | `launch spam_matter` | Matter commissioning onboarding spoofing |

The attack starts as soon as you enter a mode; the screen shows a scanning
animation and reports connecting peers. Press the **LEFT** button to stop and
exit — this halts advertising and re-initializes WiFi in station mode.

> **Saturate** replaces the former "spam all" mode. It rotates payloads for
> every platform family at the fastest legal pacing to measure how a lab
> receiver's scanner handles a saturated advertising channel. Use it only
> against your own equipment.

BLE advertising is refused when free heap is below 18 KB and stops below 8 KB
during a run, which can happen after using Thread Sniffer/Broadcast. If a mode
does not start, exit heavy radio apps first to free the heap.

# Wardriving File Formats
# Wifi 
- **MAC**: MAC of the Access Point
- **SSID**: Name of the Access Point
- **AuthMode**: Auth type
- **FirstSeen**: Date time
- **Channel**: Channel
- **Frequency**: Frequency of the AP
- **RSSI**: Signal strength
```csv
ElecCats-1.0,appRelease=1.1.6.0,model=MININO,release=1.1.6.0,device=MININO,display=SH1106 OLED,board=ESP32C6,brand=Electronic Cats,star=Sol,body=3,subBody=0
MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
84:d8:1b:9f:6f:cc,Hacknet_EXT,WPA_WPA2_PSK,2025-1-24 20:8:18,5,2432,-28,20.644129,-100.461815,1892.849976,1.500000,,,WIFI
```
# Zigbee

> [!NOTE]
> Zigbee apps are included in every profile.

Available apps: `Spoofing > Switch`, `Spoofing > Light`, `Sniffer`,
`Mesh Topology` (`launch zbmesh`) and `GPS > Wardriving > Zigbee Start`
(`launch warbee`).

`zbmesh` writes `/sdcard/zigbee/mesh_topology.csv`:

```csv
Timestamp,Channel,PAN_ID,Src_Short,Dst_Short,Frame_Type,LQI,RSSI,Packets
```

- **SourcePanID**: Source PAN ID
- **SourceADDR**: Source address
- **DestinationADDR**: Destination short address
- **Channel**: Channel
- **RSSI**: Signal strength
```csv
ElecCats-1.0,appRelease=1.1.6.0,model=MININO,release=1.1.6.0,device=MININO,display=SH1106 OLED,board=ESP32C6,brand=Electronic Cats,star=Sol,body=3,subBody=0
SourcePanID,SourceADDR,DestinationADDR,Channel,RSSI,SecurityEnabled,FrameType,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type
0x7668,5a:d2:cf:46:fc:ae:75:d7,0xffff,16,-25,No,Data,4.130518,4.130518,1907.400024,1.500000,,,Zigbee
```
# Thread

> [!NOTE]
> Thread apps are included in every profile.
> Available apps: `Sniffer`
> (`launch thread_sniffer`), `Broadcast` (`launch broadcast`) and
> `GPS > Wardriving > Thread Start` (`launch warthread`).
>
> `warthread` writes `/sdcard/thread/WarThread_<timestamp>.csv`.

- **DestinationPAN**: Destination PAN ID
- **Destination**: Destination short address
- **ExtendedSource**: Extended source address
- **Channel**: Capture channel
- **UDPSource**: UDP source port
- **UDPDestination**: UDP destination port
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

Run `launch warthread` from the console or `GPS > Wardriving > Thread Start`
from the menu. A GPS fix is required before rows are written to
`/sdcard/thread/WarThread_<timestamp>.csv`.

For a smoke test, verify that `launch warthread` prints
`Thread Wardriving module begin`, that `exit_app` returns to `minino>`, and that
WiFi is initialized again. BLE advertising is intentionally refused when free
heap is below 18 KB, which can happen after using Thread Sniffer/Broadcast.

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

> Some development board configurations may not work over the USB connection; use the UART serial interface instead.

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
# After a moment, check the device state; when the state is 'leader', the network is ready
> state
leader
Done
>
```

**Commands**:
- `dataset init new`: Create a new network
- `dataset`: Show the network details
- `dataset commit active`: Apply the configuration and set it as active
- `ifconfig up`: Activate the PHY interface
- `thread start`: Activate the Thread service
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
- `dataset networkkey`: Set the network key to match the Border Router network
- `dataset commit active`: Apply the configuration and set it as active
- `ifconfig up`: Activate the PHY interface
- `thread start`: Activate the Thread service
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

The Border Router received:
```shell
> 9 bytes from fd96:26d8:408f:b50e:bca0:8fb6:ed5a:4a27 49154 CatsRules
```


## Modbus

> ⚠️ Warning: These commands are based on my specific setup. Your configuration may differ, so make sure to adjust the IP address, request type, registers, and coils according to your environment.

> **Important**: The registers and coils depend on your setup; do not assume mine will work for you.

```bash
# List the available Access Points
list
# Connect to your Access Point
connect 0
# Set the server IP and Port (two arguments: <ip> <port>)
mb_engine_set_server 192.168.0.250 502
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

> For more information, use the `help` command to view details about each command.


## GATTCMD
A command-line utility to interact with Bluetooth GATT Characteristics:
- `gattcmd_scan`: Scan for MAC addresses
- `gattcmd_enum`: Enumerate characteristics from the device
- `gattcmd_write`: Write a value to a GATT characteristic

> This app is still under development; all testing has been performed on devices without encryption for GATT writes.

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
The AP name, password, IP configuration, and more can now be set directly through the menuconfig interface without modifying the source code.

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

The value `idf.py menuconfig > Component config > HTTP Server > (1024) Max HTTP Request Header Length` is set to 1024 to avoid issues with Android devices.

# Zigbee CLI

> [!WARNING]
> **Not available in this beta version**: Zigbee components do not work in this beta version of Minino. These components are only available in versions prior to 1.1.13.0.
>
> **To use Zigbee components**: If you need to use Zigbee functionality, you must checkout the version 1.1.13.0 using:
> ```bash
> git checkout 1.1.13.0
> ```

## Important
**To use the CLI, enable it in `Settings -> ZB CLI`. If you want to use other Zigbee apps (such as the sniffer), disable the CLI first.**

## Zigbee test connections
This test code shows how to configure a Zigbee device and use it as Home Automation (HA) Device Types, such as On/Off Switch, Window Covering, Door Lock, and On/Off Light Device.

## Hardware Required
* Minino
* Another development board with ESP32-H2 SoC as communication peer (loaded with esp_zigbee_all_device_types_app example or other examples).

## Play with All Device Type Example
This section demonstrates the process to create HA `on_off_light` and `on_off_switch` devices.

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

`zcl send_gen read -d 0xe643 --dst-ep 1 -e 2 -c 6 -a 0`


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
- **Network discovery**: `network scan -m 0x07fff800`
- **Network energy detection**: `network ed_scan -m 0x07fff800`
- **Discover Attributes**: `zcl send_gen disc_attr -d 0xe643 --dst-ep 1 -e 2 -c 4`
- **Request Active Endpoint**: `zdo request active_ep -d 0xe643`
- **Request Neighbors**: `zdo request neighbors -d 0xe643`
`zdo request neighbors -d 0x0000`

# Change log

## Unreleased
### Added
- SDR (Software Defined Radio) app shipped in the default firmware (`sdr_capture`, `sdr_host`), synced with the ESPARGOS web viewer
- BLE Saturate mode in `bt_spam` (replaces the former "spam all"), with updated menu navigation
- Thread wardriving (`warthread`) with stabilized stack lifecycle and an optimized sniffer
- Zigbee mesh topology mapper (`zbmesh`) and Zigbee wardriving (`warbee`)
- New menu icons for Mesh Topology and improved Zigbee/Thread representations
- Combined default firmware bundling SDR + Zigbee/Thread in one image, with `make builds` cross-building the per-event board profiles (`minino`, `bsides`, `dragonjar`, `ekoparty`, `bugcon`, `bsseattle`)

### Changed
- The OTA and Web File Browser access points use the password `Cats1234` by default (`CONFIG_WIFI_AP_PASSWORD`). If the password is set to an empty string, the AP is open instead of aborting
- Leaving Modbus TCP or CatDoS disconnects from the target AP, so the next session asks to connect again
- Console `launch` and `sdr_capture` close the running app before starting the new one
- Leaving wardriving waits for the scan in progress to finish (usually 1–3 s)
- After a Wi-Fi app exits, the Wi-Fi driver is shut down; the next Wi-Fi app starts it again

### Fixed
- `bt_spam`: guard BLE advertising against low heap and release the lease when restarting a mode
- Thread: release OpenThread after the sniffer exits
- Harden BLE, Zigbee and Thread feature implementations
- The back button leaves the Wi-Fi analyzer, captive portal, deauth detector, Drone ID, DroneID scanner, SSID spam, CatDoS, I2C scanner, OTA, file manager, HID and Flock instead of freezing the screen
- SSID spam: no reboot when picking an SSID list, or when the list is empty
- OTA: no abort on entry; leaving OTA stops its access point, HTTP server and tasks
- Deauth: the manager AP stays up for the whole session and stops when the app exits
- RF spectrum: `spectrum_pcap on` records again
- Wi-Fi analyzer: starting a capture while BLE holds the radio no longer hangs the board; fixed per-capture memory leaks
- No duplicate-netif abort when switching between Wi-Fi apps; Wi-Fi apps no longer start with a stopped driver after Zigbee/Thread
- BLE apps are no longer locked out after Drone ID or a Wi-Fi analyzer capture
- UART bridge: the USB console works again after leaving the bridge
- GPS, wardriving, warthread and OpenThread: safe teardown order on exit; a failed GPS start no longer blocks GPS until reboot
- Task registry no longer fills up after repeated captures or GPS sessions

### Known issues
- Some apps still force-delete their worker tasks on exit (deauth scan, DroneID, captive scan, Drone ID spoofer)
- CatDoS attack threads are never joined and leak on every attack
- BLE spam stops twice on exit
- Some apps (rf_spectrum, DroneID scanner, Drone ID, BLE spam, Modbus TCP/CatDoS, Zigbee, Thread) still drive Wi-Fi outside the radio arbiter

## v1.1.16.1
### Fixed
- Modbus TCP: memory leaks in the DoS/Write preferences, UI freeze when exiting the menu, blocking connect moved to a task, and the orphaned "connecting"/Run animation
- USB console: enabled line editing and fixed typing stalls

## v1.1.16.0
### Added
- Native SDR engine: 80 MSPS transceiver, Web Host mode synced with the ESPARGOS WebSDR viewer, GAIN negotiation and USB stability
- RF Spectrum analyzer with 2× peak tracker, smart anti-jammer and EMA filtering
- `radio_lease` arbiter, `wifi_service` and GAP dispatcher in the core
- Selectable OLED menu view styles, new menu icons, `minino_app` lifecycle and input queue
- Automatic unified `minino-merged.bin`/`.hex` on every build; SDR build profile, flash-fit checks and automated host tests in CI
### Changed
- Translated all firmware comments, headers, SDR test scripts and technical docs to English

## v1.1.15.2
### Fixed
- Hardened Wi-Fi sniffing and wardriving for stability
- Scanner OLED display and data rendering

## v1.1.15.1
### Changed
- Localized Spanish UI text to English
### Fixed
- Code formatting in HID, animations and screen-saver modules

## v1.1.15.0
### Added
- Surveillance detector app: scoring engine with decay/cooldown/tier dedupe, radio sniffer, SSID/OUI/IE/BLE matchers and a microSD signature-overlay parser
- OTA firmware updates over an HTTP server
- WiFi Spectrum Suite tooling: wardriving analyzer, spoofing analysis, shell completion and unit tests
- BLE power profiles, heap stats and GAP dispatcher
- Trackers UI redesign with an RSSI meter and GPS coordinates
### Fixed
- `bt_spam`: controller crash, GAP sync race, starting freeze, safe semaphore lifecycle, power fallback and WiFi teardown
- Memory leaks in `ssid_spam`; task manager, memory monitor and preferences safety
- Surveillance display freeze and `esp_restart` loop on back exit
### Security
- Web server: XSS and directory-traversal protections

## v1.1.14.1
### Added
- Zigbee light with LED feedback, runtime channel selection and `radio_selector` integration; Zigbee/Thread modules restored into the build
### Changed
- Updated the ESP-IDF requirement to 5.5.1
### Fixed
- Broad memory and task-handle hardening across WiFi, thread sniffer, trackers, captive, warbee, animations, buzzer and zigbee modules
- DroneID packet queue/processing; empty SSID-list handling in spam; recursive mutex for animations

## v1.1.14.0-beta
### Added
- Task manager with error handler and memory monitor (CatDoS, LED events, GPS)
- USB-UART bridge app
### Changed
- CI: code-review workflow and additional tag-naming formats; ESP-IDF version bump in the Dockerfile
### Fixed
- Concurrency safety and memory handling; overflow condition; build warnings

## v1.1.13.0
### Added
- GPS config menu with update rate and test mode, UTC labels and a satellite-count running animation
- Delete button in the web file manager

## v1.1.12.0
### Added
- BLE recon app with a MAC-address filter and advertisement scanner
- Firmware version shown on device; captive-portal filter file
### Fixed
- Wardriving crashes (double init and >200 packets); SSID with comma; file manager after wardriving; OLED cleared after file transfer; one GPX file per session

## v1.1.11.0
### Added
- Zigbee CLI integrated into the app with an interactive screen and ZB CLI setting; Zigbee sniffer and switch
- Captive portal from SD card with user-input capture/storage, SSID replicate and optional dump to SD
- GATT recon/scan/write commands, a command to stop BLE services and graceful Ctrl+C shutdown
- WiFi scan & list-SSID command with RSSI; `println` command
### Fixed
- Wardriving packets >200 and reset-to-0; SD write and URL decoding; Modbus engine request handling

## v1.1.10.0
### Added
- BLE GATT write via the gattcmd tool; BLE enumerate (gattc) and scan via console
- HID controls: mute, next/prev track and stop, with scrolling in the HID controls menu
### Fixed
- GATT write functionality and table formatting; menu/scroll issues; drone scan

## v1.1.9.0
### Added
- Modbus TCP attack suite: writer/DoS commands, screens, preferences and runtime parameters
- BLE DroneID spoofing toggle from the settings menu
- PCB housing (hardware)
### Fixed
- CatDoS animations, notification and disconnect callback; WiFi settings modal and submenus

## v1.1.8.0
### Added
- DroneID scanner and multi-drone spoofing with runtime settings (count, channel, location), a settings menu and saved preferences
- BSides Seattle board compatibility
### Fixed
- Memory allocation; channel validation and range; DroneID screen and reload time

## v1.1.7.2
### Added
- WiFi region / country-code configuration menu
- Release automation: all boards built on a new tag, zipped release files and a draft release with flashing instructions
### Fixed
- GPIO menu conditions

## v1.1.7.1
### Added
- `make builds` and a binaries-merge Make command; renamed partition tables and `get_builds.sh`
- Automatic release on new tags; 8 MB flash as default; version read from the build script
### Changed
- Apps disabled by technology; OTA hidden on badges

## v1.1.7.0
### Added
- SSID spam command: Custom SSID lists for beacon flooding attacks
- Wardriving Thread protocol support
- Flash storage module to handle lists of values
