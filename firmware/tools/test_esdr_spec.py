#!/usr/bin/env python3
"""Test of spectrum mode (SPEC) against the board.

Reproduces what the ESPARGOS viewer does when selecting "On-Chip Spectrum":
queries profiles, requests the spectrum stream, and decodes SPC1 frames just
like the viewer (header, natural-order bins, and CRC32).

usage:  python3 test_esdr_spec.py [port]
"""
import re
import struct
import sys
import time
import zlib

import serial

PORT = "/dev/ttyACM0"
if len(sys.argv) > 1:
    PORT = sys.argv[1]

OK = 0
FAIL = 0


def check(name, cond, detail=""):
    global OK, FAIL
    if cond:
        OK += 1
        print(f"  \033[32mPASS\033[0m  {name}" + (f"   {detail}" if detail else ""))
    else:
        FAIL += 1
        print(f"  \033[31mFAIL\033[0m  {name}" + (f"   {detail}" if detail else ""))


class Client:
    def __init__(self, port):
        self.ser = serial.Serial(port, 115200, timeout=5)
        self.ser.dtr = False
        self.ser.rts = False
        self.queue = b""

    def write(self, data):
        self.ser.write(data)
        self.ser.flush()

    def command(self, s):
        self.write((s + "\n").encode())

    def read(self, n, deadline=None):
        deadline = deadline or (time.perf_counter() + 10)
        out = b""
        while len(out) < n:
            if time.perf_counter() > deadline:
                raise TimeoutError(f"requested {n} bytes, got {len(out)}")
            chunk = self.ser.read(min(1024, n - len(out)))
            if chunk:
                out += chunk
        return out

    def line(self, prefix="", limit=4096):
        s = ""
        end = time.perf_counter() + 10
        for _ in range(limit):
            b = self.read(1, end)[0]
            if b == 10:
                s = s.strip()
                if not s.startswith(prefix):
                    raise ValueError(f"unexpected response: {s!r}")
                return s
            s += chr(b)
        raise ValueError("response too long")


def decode_frames(data):
    """The viewer's SpectrumDecoder: scans for valid SPC1 frames."""
    frames = []
    i = 0
    while i + 28 <= len(data):
        if data[i:i + 4] != b"SPC1":
            i += 1
            continue
        if i + 32 > len(data):
            break
        bins = data[i + 26]
        n = 1 << bins
        total = 28 + n + 4
        if i + total > len(data):
            break
        body = data[i:i + 28 + n]
        crc = struct.unpack_from("<I", data, i + 28 + n)[0]
        if zlib.crc32(body) != crc:
            i += 1
            continue
        frames.append({
            "seq": struct.unpack_from("<I", data, i + 4)[0],
            "index": struct.unpack_from("<Q", data, i + 8)[0],
            "samples": struct.unpack_from("<I", data, i + 16)[0],
            "ffts": struct.unpack_from("<H", data, i + 20)[0],
            "flags": data[i + 22],
            "gain": data[i + 23],
            "log2n": bins,
            "dbmul": data[i + 27],
            "bins": list(data[i + 28:i + 28 + n]),
        })
        i += total
    return frames


print(f"\n=== Spectrum mode on board ({PORT}) ===\n")
c = Client(PORT)

# ------------------------------------------------------------------ startup
print("[1] startup and port hand-over")
boot = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 6:
    try:
        d = c.ser.read(4096)
    except Exception:
        d = b""
    if d:
        boot += d
        if b"minino>" in boot:
            break
check("console appears", b"minino>" in boot)

c.write(b"sdr_host\r")
time.sleep(1.0)
banner = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 4:
    try:
        d = c.ser.read(4096)
    except Exception:
        d = b""
    if d:
        banner += d
        if b"comes back" in banner:
            time.sleep(0.3)
            try:
                banner += c.ser.read(4096)
            except Exception:
                pass
            break
check("host mode starts", b"comes back" in banner)

# ----------------------------------------------------------------- SPECINFO?
print("\n[2] profiles (SPECINFO?)")
c.command("SPECINFO?")
info = c.line("SPECINFO ")
print("    " + info)
check("JSON matches the shape expected by the viewer",
      '"continuous":false' in info and '"transports":[' in info and '"profiles":[' in info)
check("transport is USB", '"USB"' in info)

profiles = re.findall(r"\[(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\]", info)
check("profiles are present", len(profiles) > 0, f"{len(profiles)} profiles")
rates = {int(p[0]) for p in profiles}
check("all three C6 sample rates are present", rates == {80000000, 40000000, 16000000}, str(sorted(rates)))
sizes = {int(p[2]) for p in profiles}
check("FFT sizes reach 2048", 2048 in sizes, str(sorted(sizes)))

# The viewer picks the profile matching its requested bin count.
n = 1024
chosen = [p for p in profiles if int(p[2]) == n and int(p[0]) == 80000000]
check("has a 1024-bin profile at 80 MS/s", len(chosen) == 1, str(chosen))

# ---------------------------------------------------------------------- SPEC
print(f"\n[3] spectrum stream ({n} bins)")
rate_code = int(chosen[0][1])
stride = int(chosen[0][3])
upf = int(chosen[0][4])
c.command(f"SPEC 0 {stride} {upf} 0 {rate_code} {n}")

header = c.line("SPEC ")
parts = header.split(" ")
check("header has five fields", len(parts) == 5, header)
check("header carries bin count", int(parts[1]) == n, parts[1])
check("header carries sample rate", int(parts[2]) == 80000000, parts[2])
check("header carries FFT size", int(parts[3]) == n, parts[3])

# Frames arrive as a stream: collect a couple and stop.
raw = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 6 and len(raw) < 4096:
    try:
        raw += c.ser.read(4096)
    except Exception:
        pass
    if len(decode_frames(raw)) >= 2:
        break

frames = decode_frames(raw)
check("valid frames arrive", len(frames) > 0, f"{len(frames)} frames, {len(raw)} bytes")
if frames:
    f = frames[0]
    check("frame has SPC1 magic", raw[:4] == b"SPC1")
    check("frame bin count matches request", f["log2n"] == (n.bit_length() - 1))
    check("dB multiplier is 2", f["dbmul"] == 2)
    check("gap flag bit is set (non-continuous captures)", (f["flags"] & 8) != 0,
          f"flags {f['flags']}")
    check("bins are codes from 0 to 255",
          all(0 <= b <= 255 for b in f["bins"]) and len(f["bins"]) == n)
    check("bins have signal (not all zero)", max(f["bins"]) > 0,
          f"max {max(f['bins'])}, min {min(f['bins'])}")
    # Bin 0 is the LO carrier, though it does not have to be the strongest
    # when Wi-Fi networks are nearby. Check that the spectrum has structure.
    ordered = sorted(f["bins"], reverse=True)
    median = ordered[len(ordered) // 2]
    check("spectrum has structure (not flat noise)",
          ordered[0] > median + 10,
          f"max {ordered[0]}, median {median}, min {ordered[-1]}")
    check("LO carrier is present at bin 0", f["bins"][0] > 0,
          f"center {f['bins'][0]}")

# ------------------------------------------------------------------- stop
print("\n[4] stopping stream")
c.command("")  # empty line: how the viewer stops the stream
time.sleep(1.0)
end = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 5:
    try:
        d = c.ser.read(4096)
    except Exception:
        d = b""
    if d:
        end += d
    if b"SPECEND" in end:
        break
check("firmware replies with SPECEND", b"SPECEND" in end)
if b"SPECEND" in end:
    # SPECEND follows the binary frames, so search the byte stream rather than
    # reading line by line.
    tail = end[end.index(b"SPECEND"):]
    line = tail[:tail.find(b"\n")].decode("utf-8", "replace")
    fields = line.split()
    # SPECEND status detail units pairs elapsed_us late_max work_max_cycles
    #         frames drops abandoned ffts stopped_by_host
    check("SPECEND has twelve fields", len(fields) == 13, line[:90])
    check("status is 0 (normal stop)", fields[1] == "0", fields[1])
    check("FFTs were computed", int(fields[3]) > 0, fields[3])
    check("sample pairs were processed", int(fields[4]) > 0, fields[4])
    check("frames were emitted", int(fields[8]) > 0, fields[8])
    check("host stopped the stream", fields[12] == "1", fields[12])

print(f"\n=== {OK} passed, {FAIL} failed ===\n")
sys.exit(1 if FAIL else 0)
