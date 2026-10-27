# The SDR Wire Protocol: What Is Sent, How Spectrum Receives It, and Where It Appears

> The two packet types exchanged between the host and the board, including
> the real byte sequences captured on both Minino units and where each stream
> is rendered on screen.
>
> - **Date:** 2026-10-21
> - **Boards:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` and rev v0.1
>   `7c:2c:67:42:84:a0`
> - **Applications:** Only the two SDR modes: the native app (burst capture)
>   and spectrum mode. No radiating applications active.
> - **Related Documents:** Results for both applications in
>   [`spectrum-SDR-results.md`](./spectrum-SDR-results.md), full test report in
>   [`sdr-resultados.md`](./sdr-resultados.md), user guide in
>   [`sdr-guia.md`](./sdr-guia.md), and design reference in
>   [`esp-sdr-integration.md`](./esp-sdr-integration.md)

To capture the live wire exchange:

```bash
python3 tools/test_esdr_wire.py /dev/ttyACM0
```

---

## 1. Transport Layer

The transport channel is the ESP32-C6 **USB Serial/JTAG** peripheral, exposed by
the Minino driver as a virtual serial port (CDC-ACM). Three implementation
details govern its behavior:

1. **Port handover is performed by the console task.** The REPL echoes incoming
   characters (`linenoise`), whereas the wire protocol requires clean responses.
   Therefore, the `sdr_host` command executes *inside* the console task: when
   the session starts, the console is paused between two reads so the serial
   port has a single reader.

   ```
   minino> sdr_host
   Serving the ESP-SDR protocol on the USB port.
   Open https://espargos.net/espsdr/app/ and connect to this board.
   The console comes back when the viewer sends RELEASE, or type MININO.
   ```

2. **All logging is silenced during an active session.** The master log level is
   set to `ESP_LOG_NONE`: a single log line emitted in the middle of a binary
   payload would corrupt the frame. The radio is initialized *before* silencing
   the log because the Wi-Fi driver prints directly from ROM and bypasses
   `esp_log`.

3. **Payloads are transmitted in 64-byte chunks.** The USB Serial/JTAG ring
   buffer is 256 bytes wide, and `xRingbufferSend()` rejects blocks larger than
   the ring buffer capacity: attempting to send a 40,950-byte payload in a
   single call waits indefinitely for space that never exists and writes
   nothing. Consequently, `port_write()` splits transfers into chunks
   (`ESDR_TX_CHUNK 64`).

Protocol lines end with **LF**; **console** commands (`sdr_host`,
`sdr_capture`) end with **CR**.

---

## 2. Burst Capture Packet (Viewer "Burst" Mode)

### 2.1 Text Handshake

All commands and responses during this phase are LF-terminated ASCII lines:

```
-> SYNC 1792629170881
<- SYNC 1792629170881
-> INFO
<- C6SDR 6 burst 16380
-> CAPS
<- CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16 UARTBAUD SPEC SPECN SPECCAPS DCT
-> RANGE?
<- RANGE 2412 2472 1
-> FREQ 2412
<- OK
```

| Response | Meaning to the Viewer |
|---|---|
| `SYNC <nonce>` | Echoes the nonce: confirms the board is responsive and the serial buffer is clean |
| `C6SDR 6 burst 16380` | Chip family and maximum burst sample count (the viewer requires a value between 4096 and 16384) |
| `CAPS ...` | Capability tokens. The viewer hides UI controls for capabilities **not** listed here: `GAIN`, `ALPF`, and `GPIO` are omitted, so their controls remain hidden |
| `RANGE 2412 2472 1` | Tuning range in MHz and step size (Wi-Fi channels: the dump engine borrows the Wi-Fi PHY) |
| `OK` | Tuning succeeded |

### 2.2 Capture Header

```
-> CAP20 4096 0
<- DATA 4096 31145255 65
```

`DATA <samples> <crc32> <micros>`:

| Field | Value | Description |
|---|---|---|
| samples | 4096 | Number of I/Q sample pairs in the burst |
| crc32 | `31145255` | Hexadecimal CRC32 (zlib) checksum of the binary payload |
| micros | 65 | On-chip capture duration in microseconds |

The viewer validates this header against a strict regular expression
(`/^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/`) and then verifies the payload CRC.

### 2.3 Binary Payload

| Format | Bytes per Pair | Packing Layout |
|---|---|---|
| `CAP16` | 2 | Signed 8-bit I followed by signed 8-bit Q |
| `CAP20` | 2.5 | 20 bits per pair, least significant bit first: **I in bits 0-9 and Q in bits 10-19** |

The raw 32-bit word from the hardware dump engine places I in bits 9-0 and Q in
bits 19-10, so `CAP20` packs the lower 20 bits directly:

```
    31        28 27        20 19        10 9         0
   +------------+------------+------------+------------+
   |  (unused)  |    gain    |     Q      |     I      |
   |            |   index    | (10 bits)  | (10 bits)  |
   +------------+------------+------------+------------+
```

First bytes of a 4096-sample `CAP20` payload (10,240 bytes total) as sent by
board `48:f6:ee`:

```
000000  5a f8 3f 86 fd 70 80 7f 07 f3 72 08 4f 06 f2 52  Z.?..p....r.O..R
000010  58 af 84 f9 55 b8 bf 06 fe 80 08 00 c9 01 8d f4  X...U...........
000020  ef 47 fa 6b 54 ef c5 f3 5d 7c 3f 06 fe 68 40 60  .G.kT...]|?..h@`
... (10240 bytes total)
```

And the CRC32 advertised in the header matches the payload checksum:

```
payload CRC32 (zlib): 31145255   (advertised in header: 31145255)
```

The second board (`7c:2c:67`), given the identical request:

```
-> CAP20 4096 0
<- DATA 4096 1a58a795 64
000000  31 7c d1 c3 14 2d dc c0 00 05 ee 17 f0 bd 02 dd  1|...-..........
000010  b7 70 be 15 f5 bf 01 c0 1b 09 68 01 41 10 10 dc  .p........h.A...
000020  00 81 0e 11 08 41 c1 11 24 20 d1 43 11 57 f8 50  .....A..$ .C.W.P
... (10240 bytes total)
payload CRC32 (zlib): 1a58a795   (advertised in header: 1a58a795)
```

Same format and payload length, different live RF samples, and matching CRC32
on both boards.

> The CRC32 uses the standard zlib polynomial and conventions, computed via a
> **16-entry lookup table** (nibble by nibble): 64 bytes of flash instead of a
> 1024-byte table, saving precious memory in a tight build.

---

## 3. Spectrum Packet ("On-Chip Spectrum" Mode)

In this mode, the board does not send raw time-domain samples: **it computes
the FFT on-chip** and transmits power bin codes. This allows the web viewer to
refresh at dozens of frames per second instead of around ten.

### 3.1 Stream Header

```
-> SPECINFO?
<- SPECINFO {"continuous":false,"transports":["USB"],"profiles":[[80000000,0,256,1,1,0],[80000000,0,512,1,1,0],[80000000,0,1024,1,1,0],[80000000,0,2048,1,1,0],[40000000,1,256,1,1,0],...
-> SPEC 0 1 1 0 1 1024
<- SPEC 1024 40000000 1024 2412
```

`SPECINFO` advertises all **12 profiles**: the three ESP32-C6 sample rates (80,
40, and 16 MS/s) x four FFT sizes (256, 512, 1024, and 2048 bins), along with
rate code, step, units, detector mode, and bin count.

`SPEC <ms> <step> <units> <det> <rate> <bins>` starts the stream, and the board
replies with `SPEC <bins> <rate_hz> <bins> <mhz>`: bin count, sample rate in
Hz, bin count again, and the tuned center frequency in MHz.

### 3.2 The `SPC1` Frame

Each frame consists of a 28-byte header, the FFT bins in natural frequency
order, and a 4-byte CRC32 trailer. Real bytes captured from a 1024-bin frame
(1,056 bytes total):

```
000000  53 50 43 31 00 00 00 00 00 6d d6 3e 00 00 00 00  SPC1.....m.>....
000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 50 4f 4f  .......N.....POO
00001c  0c 50 4f 4f 4d 3a 44 48 4b 4f 4a 26 4a 40 3a 4e  .POOM:DHKOJ&J@:N
00002c  50 50 49 3e 48 45 47 47 3b 46 4a 4a 4e 47 37 39  PPI>HEGG;FJJNG79
```

| Offset | Bytes | Field |
|---|---|---|
| 0 | 4 | `SPC1` magic signature |
| 4 | 4 | Frame sequence number |
| 8 | 8 | Index of the first sample |
| 16 | 4 | Number of samples spanned by the frame (1024) |
| 20 | 2 | Number of aggregated FFTs (1) |
| 22 | 1 | Flags: bit 0 max detector, **bit 3 gaps between captures** (`08`) |
| 23 | 1 | Gain metadata (`4e` = 78) |
| 24 | 2 | Cumulative dropped frames (`0000`) |
| 26 | 1 | `log2` of bin count (`0a` = 10 -> 1024) |
| 27 | 1 | Power code dB multiplier (`02`) |
| 28 | N | FFT bins in natural order, 1 byte per bin |
| 28+N | 4 | CRC32 of the header plus bins |

Verified CRC32:

```
header + bins CRC32: ce794789   (frame trailer: ce794789)
```

The corresponding frame on the second board (`7c:2c:67`):

```
000000  53 50 43 31 00 00 00 00 00 00 00 00 00 00 00 00  SPC1............
000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 48 00 00  .......N.....H..
00001c  0c 48 00 00 00 57 4d c0 d7 4d 4d 3e ff 00 43 00  .H...WM..MM>..C.
00002c  4d 4a 9a ff ff 00 25 45 45 40 4a 39 3f ff 40 00  MJ....%EE@J9?.@.
header + bins CRC32: 5367f516   (frame trailer: 5367f516)
```

The signature, sizes, `log2` bin count (`0a`), multiplier (`02`), and gap flag
bit (`08`) are identical across both boards.

**Each bin code** represents `20*log10(power)` in the Q15 domain, clamped to
`0..255`, and the viewer multiplies it by byte 27 (`2`) to render the value in
dB.

### 3.3 Stream Termination

An empty line terminates the spectrum stream (sent by the viewer when closing
or stopping), and the board replies with the stream summary:

```
-> (empty line)
<- SPECEND 0 0 61 62464 27109 0 0 61 0 0 61 1
```

| Field | Value | Description |
|---|---|---|
| status | 0 | Normal stop |
| detail | 0 | No error |
| units | 61 | Capture units delivered |
| pairs | 62,464 | Sample pairs processed |
| elapsed_us | 27,109 | Stream duration |
| late_max | 0 | Maximum scheduling lateness |
| work_max_cycles | 0 | Maximum processing cycles per frame |
| frames | 61 | Total frames sent |
| drops | 0 | Dropped frames |
| abandoned | 0 | Abandoned captures |
| ffts | 61 | Total FFTs computed |
| stopped_by_host | 1 | Stream stopped by host request |

---

## 4. How the Firmware Processes Commands

### 4.1 Command Parser

Each command arrives as an LF-terminated line and is parsed according to the
ESPARGOS firmware rules: a command with trailing garbage is rejected, and an
empty line is not treated as a command. Every valid command produces a
response; unrecognized commands return `ERR command`.

`SPEC` is passed as a full line to the spectrum module, which validates its
parameters:

```
SPEC <ms> <step> <units> <det> <rate> <bins> [stats]
```

| Rule | Reason |
|---|---|
| `step` must be 1 | The hardware dump engine does not support other step values |
| `units` from 1 to 8 | Number of FFT windows aggregated per frame |
| `det` is 0 or 1 | Peak/max (0) or average (1) detector |
| `rate` must match an ESP32-C6 rate code | 0 = 80 MS/s, 1 = 40 MS/s, 6 = 16 MS/s |
| `bins` from 256 to 2048 (power of two) | On-chip FFT size limits |

A `SPEC` command with invalid arguments returns `ERR spec_args`, and if heap
allocation for the FFT workspace fails, it returns `ERR spectrum_memory`.

### 4.2 On-Chip FFT Implementation

- **Decimation-in-Frequency (DIF) Radix-2** with bit-reversed output permutation.
- **Every butterfly stage divides by two**: without per-stage scaling, a strong
  tone would saturate Q15 arithmetic in the first stage. Overall transform gain
  is $1/n$.
- Output bins are emitted in **natural frequency order** (DC/LO at bin 0,
  followed by positive and negative offsets as expected by the viewer): a
  bit-reversed spectrum looks deceptively plausible on screen while representing
  scrambled frequencies. Host unit tests verify the output against a direct
  reference DFT.

### 4.3 Frame Cadence and Watchdog Yielding

The stream is not gapless: between captures, the hardware dump engine must be
re-armed, which is why frames set **bit 3 of the flags byte** (indicating gaps
between captures). The web viewer displays this as *"Reception has gaps between
snapshots"*.

The streaming loop yields to the FreeRTOS idle task every 20 ms: the idle task
feeds the task watchdog timer, so failing to yield during a long spectrum
session would trigger a watchdog reset.

Measured on both boards (40 MS/s, 1024 bins): **1,399 to 1,430 frames in
12 s**, with 100% valid CRCs, `drops = 0`, and `abandoned = 0`.

---

## 5. Where Each Stream Appears

### 5.1 ESPARGOS Web Viewer (<https://espargos.net/espsdr/app/>)

The viewer provides two operating modes, each consuming one of the two packet
formats:

| Viewer Mode | Command Sent | What Is Displayed |
|---|---|---|
| **Burst** (waterfall mode) | `CAP16` / `CAP20` | The browser computes the FFT from raw I/Q samples and renders the spectrum waterfall and time-domain I/Q plots |
| **On-Chip Spectrum** | `SPEC` | The pre-computed on-chip FFT spectrum using 1-byte power codes, refreshing at dozens of frames per second |

The handshake follows Section 2.1, and the viewer selects the `SPECINFO`
profile matching the requested bin count. Upon closing, it sends `RELEASE` and
the Minino serial console is restored.

### 5.2 Minino Native App (`Applications > SDR`)

The 128x64 OLED screen displays:

| Line / Region | Content |
|---|---|
| Page 0 (inverted) | `MININO SDR [IDL]` (or `[RX]` while capturing) |
| Page 2 | `FREQ: 2412 MHz` |
| Page 3 | `RATE: 80 MS/s IQ` |
| Page 4 | `CAPS: 31     PKTS` (captured burst count) |
| y=42..52 | Signal level meter: a rectangular frame with an internal RSSI bar |
| Page 7 | `^v:Tune <:Exit` |

After each capture, line 6 shows the capture summary:

```
CAP 16380 smp 218 us
```

and line 7 shows the storage result: `iQ saved to SD` or `no SD: capture only`.

Button controls: **UP/DOWN** tune in ±5 MHz steps (2412 to 2472 MHz), **RIGHT**
triggers a capture, and **LEFT** exits (releasing the radio back to Minino).

### 5.3 Minino RF Analyzer (`Applications > Spectrum`)

Note that `Applications > Spectrum` is **not** the SDR dump app: it is Minino's
promiscuous Wi-Fi/Zigbee channel analyzer, which plots **per-channel** power
using packet RSSI from the Wi-Fi driver (displaying channel, dBm, threshold,
the -90/-70/-50/-30 scale, Fox Hunt proximity, and DroneID alerts). The true
wideband I/Q FFT engine is the SDR module described in Section 5.2.

---

## 6. Results Using Exclusively These Two SDR Modes

| Suite | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| `tools/test_esdr_web.py` (viewer protocol) | 56/56 | 56/56 |
| `tools/test_esdr_spec.py` (spectrum mode) | 28/28 | 28/28 |
| `tools/test_esdr_app.py` (native app) | 16/16 | 16/16 |

Simultaneous dual-board runs (`tools/test_esdr_pair.py`):

| Assignment | Board A | Board B | Result |
|---|---|---|---|
| 1 | Native app | Spectrum | App: 3 bursts of 16,380 words (218, 217, 218 us); Spectrum: 1,416 frames with valid CRC |
| 2 | Spectrum | Native app | Spectrum: 1,399 frames; App: 3 bursts (217, 218, 218 us) |
| 3 | Native app | Native app | 3 bursts on each board (one took 557 us) |
| 4 | Spectrum | Spectrum | 1,413 and 1,402 frames, with both profiles compared band by band |

Full details and complete tables are available in
[`sdr-resultados.md`](./sdr-resultados.md).
