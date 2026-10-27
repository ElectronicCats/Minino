#!/usr/bin/env python3
"""Wire-level inspection, byte by byte: what the board sends and receives.

Only the two SDR modes are used: host mode (which also serves spectrum mode)
and burst capture. The script dumps the actual port exchange in hexadecimal
to inspect the format of each packet.

    usage: test_esdr_wire.py <port>
"""
import os
import re
import select
import sys
import termios
import time
import tty
import zlib


class Wire:
    """Raw serial port without touching DTR/RTS (some USB stacks stall on it)."""

    def __init__(self, port, baud=115200):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        attrs = termios.tcgetattr(self.fd)
        attrs[2] &= ~(termios.CSIZE | termios.PARENB)
        attrs[2] |= termios.CS8
        attrs[4] = baud
        attrs[5] = baud
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def write(self, data):
        if isinstance(data, str):
            data = data.encode()
        os.write(self.fd, data)

    def read(self, seconds=1.0):
        out = b""
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            r, _, _ = select.select([self.fd], [], [], 0.2)
            if not r:
                continue
            try:
                d = os.read(self.fd, 4096)
            except OSError:
                break
            if not d:
                break
            out += d
        return out

    def read_line(self, seconds=10.0):
        """Reads one line. Returns the line and any remaining bytes on the port:
        the binary payload immediately follows the LF and must not be lost."""
        out = b""
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            b = self.read(0.5)
            if not b:
                continue
            out += b
            if b"\n" in out:
                head, _, tail = out.partition(b"\n")
                return head.decode("utf-8", "replace"), tail
        return out.decode("utf-8", "replace"), b""

    def wait_console(self, tries=8):
        """Waits for the prompt. If the board was left in host mode, an empty
        line and RELEASE return it to the console."""
        seen = ""
        for _ in range(tries):
            seen += self.read(1.0).decode("utf-8", "replace")
            if "minino>" in seen:
                return True
            self.write("\n")
            time.sleep(0.4)
            seen += self.read(0.5).decode("utf-8", "replace")
            if "minino>" in seen:
                return True
            self.write("RELEASE\n")
            time.sleep(0.6)
            seen += self.read(0.6).decode("utf-8", "replace")
        return "minino>" in seen

    def close(self):
        os.close(self.fd)


def hexdump(data, offset=0, length=48):
    lines = []
    for i in range(0, min(len(data), length), 16):
        chunk = data[i:i + 16]
        hexa = " ".join(f"{b:02x}" for b in chunk)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"   {offset + i:06x}  {hexa:<47}  {text}")
    if len(data) > length:
        lines.append(f"   ... ({len(data)} bytes total)")
    return "\n".join(lines)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    w = Wire(port)
    time.sleep(0.5)
    w.read(1.0)

    print(f"\n=== Board wire trace ({port}) ===\n")

    # ---------------------------------------------------------- hand-over
    if not w.wait_console():
        print("console did not appear")
        sys.exit(1)
    banner = ""
    for _ in range(3):
        w.write("sdr_host\r")
        time.sleep(1.5)
        banner = w.read(3.0).decode("utf-8", "replace")
        if "comes back" in banner:
            break
    if "comes back" not in banner:
        print("host mode did not start:", banner[-200:])
        sys.exit(1)
    print("[1] port hand-over (console goes silent)")
    for line in banner.splitlines():
        if line.strip():
            print("   " + line)
    print("   (all board output from here on has logging muted:")
    print("    the master log level drops to ESP_LOG_NONE during the session)")

    # ------------------------------------------------------- handshake
    print("\n[2] handshake (text)")
    for cmd in ("SYNC 1792629170881", "INFO", "CAPS", "RANGE?", "FREQ 2412"):
        w.write(cmd + "\n")
        time.sleep(0.3)
        line, _ = w.read_line(5.0)
        print(f"   -> {cmd}")
        print(f"   <- {line}")

    # ------------------------------------------------------- burst
    print("\n[3] burst (CAP20): header and start of payload")
    w.write("CAP20 4096 0\n")
    time.sleep(0.3)
    header, rest = w.read_line(5.0)
    print(f"   -> CAP20 4096 0")
    print(f"   <- {header}")
    m = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
    if m:
        n = int(m.group(1))
        want = -(-(n * 20) // 8)
        data = rest
        end = time.perf_counter() + 10
        while len(data) < want and time.perf_counter() < end:
            chunk = w.read(0.5)
            if chunk:
                data += chunk
        print(f"   payload is {len(data)} bytes for {n} samples "
              f"(20 bits per pair, I in bits 0-9 and Q in bits 10-19)")
        print(f"   payload CRC32 (zlib): "
              f"{zlib.crc32(data):08x}  (advertised by header: {m.group(2)})")
        print("   first bytes of payload:")
        print(hexdump(data))

    # ------------------------------------------------------- spectrum
    print("\n[4] spectrum mode (SPEC): header and one SPC1 frame")
    w.write("SPECINFO?\n")
    time.sleep(0.3)
    info, _ = w.read_line(5.0)
    print(f"   -> SPECINFO?")
    print(f"   <- {info[:150]}...")
    w.write("SPEC 0 1 1 0 1 1024\n")
    time.sleep(0.3)
    header, rest = w.read_line(5.0)
    print(f"   -> SPEC 0 1 1 0 1 1024")
    print(f"   <- {header}")
    # one full frame
    raw = rest
    end = time.perf_counter() + 5
    while time.perf_counter() < end and len(raw) < 2048:
        chunk = w.read(0.5)
        if chunk:
            raw += chunk
    k = raw.find(b"SPC1")
    if k >= 0:
        bins = 1 << raw[k + 26]
        total = 28 + bins + 4
        frame = raw[k:k + total]
        print(f"   SPC1 frame: {total} bytes ({bins} bins)")
        print("   header (28 bytes):")
        print(hexdump(frame, 0, 28))
        print("   first bins:")
        print(hexdump(frame[28:], 28, 32))
        print("   CRC32 of header and bins: "
              f"{zlib.crc32(frame[:-4]):08x}  "
              f"(frame CRC: {int.from_bytes(frame[-4:], 'little'):08x})")
    else:
        print("   could not extract a frame from the stream")

    # ------------------------------------------------------- stop
    print("\n[5] stop stream (empty line)")
    w.write("\n")
    time.sleep(0.8)
    tail = w.read(2.0)
    k = tail.find(b"SPECEND")
    if k >= 0:
        line = tail[k:tail.find(b"\n", k)]
        print("   <- " + line.decode("utf-8", "replace"))
    w.write("RELEASE\n")
    time.sleep(0.5)
    w.read(1.0)
    w.close()


if __name__ == "__main__":
    main()
