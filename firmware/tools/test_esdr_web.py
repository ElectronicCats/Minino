#!/usr/bin/env python3
"""Tests the board against the ESPARGOS browser protocol.

This script replicates the logic of esp-web-sdr (radio.js) as faithfully as
possible: the same handshake, regular expressions, CRC verification, and I/Q
unpacking. What happens here is what the browser viewer sees.

The board must be flashed with the SDR variant (firmware/sdkconfig.sdr), which
is the only configuration that reserves the capture bank:

    make build-sdr
    idf.py -B build-sdr flash

And the port must be handed over beforehand, because the console and the
browser cannot listen on the port at the same time:

    minino> sdr_host

usage:  python3 tools/test_esdr_web.py [port] [--quick]

Requires pyserial.
"""
import json
import re
import sys
import time
import zlib

import serial

PORT = "/dev/ttyACM0"
QUICK = "--quick" in sys.argv
if len(sys.argv) > 1 and not sys.argv[1].startswith("--"):
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
    """Browser client: WebSerial at 115200 over USB."""

    def __init__(self, port):
        self.ser = serial.Serial(port, 115200, timeout=5)
        self.ser.dtr = False
        self.ser.rts = False
        self.queue = b""

    def write(self, data):
        self.ser.write(data)
        self.ser.flush()

    def read(self, n, deadline=None):
        # Read in chunks: a single 32 KB read blocks waiting for all bytes to
        # arrive and pyserial only returns when its own timeout expires.
        deadline = deadline or (time.perf_counter() + 30)
        out = b""
        while len(out) < n:
            if time.perf_counter() > deadline:
                raise TimeoutError(f"requested {n} bytes, got {len(out)}")
            want = min(1024, n - len(out))
            chunk = self.ser.read(want)
            if chunk:
                out += chunk
        return out

    def command(self, s):
        self.write((s + "\n").encode())

    def line(self, prefix="", limit=256):
        """Matches radio.js: reads until LF and strips CR."""
        s = ""
        end = time.perf_counter() + 5
        for _ in range(limit):
            b = self.read(1, end)[0]
            if b == 10:
                s = s.strip()
                if not s.startswith(prefix):
                    raise ValueError(f"unexpected response: {s!r} (expected {prefix!r})")
                return s
            s += chr(b)
        raise ValueError("response too long")

    def synchronize(self):
        """Matches radio.js: sends LF plus a marker and waits for its echo."""
        marker = "SYNC " + str(int(time.time() * 1000))
        deadline = time.perf_counter() + 4
        self.command("\n" + marker)
        window = ""
        for _ in range(70000):
            window += chr(self.read(1, deadline)[0])
            if len(window) > len(marker) + 1:
                window = window[-(len(marker) + 1):]
            if window == marker + "\n":
                return marker
        raise TimeoutError("sync marker not seen")

    def capture(self, n, rate, bits):
        """Matches radio.js: capturePacket()."""
        divider = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
                   16000000, 32000000, 10666667, 6400000].index(rate)
        self.command(f"CAP{bits * 2} {n} {divider}")
        header = self.line()
        if header.startswith("ERR "):
            raise ValueError(header)
        h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
        if not h or int(h.group(1)) != n:
            raise ValueError("invalid capture header: " + header)
        want = -(-(n * bits * 2) // 8)
        data = self.read(want, time.perf_counter() + 5)
        if zlib.crc32(data) != int(h.group(2), 16):
            raise ValueError("CRC mismatch")
        return data, int(h.group(3))


def decode_iq(data, n, bits):
    """radio.js decodeIQ(), word for word."""
    iq = []
    if bits == 8:
        for j in range(n):
            i = data[2 * j] if data[2 * j] < 128 else data[2 * j] - 256
            q = data[2 * j + 1] if data[2 * j + 1] < 128 else data[2 * j + 1] - 256
            iq.append(i / 128.0)
            iq.append(-q / 128.0)
        return iq
    acc = 0
    held = 0
    k = 0
    for j in range(n):
        while held < 20:
            acc |= data[k] << held
            k += 1
            held += 8
        word = acc & 0xFFFFF
        acc >>= 20
        held -= 20
        i = word & 1023
        q = (word >> 10) & 1023
        i = i - 1024 if i >= 512 else i
        q = q - 1024 if q >= 512 else q
        iq.append(i / 512.0)
        iq.append(-q / 512.0)
    return iq


def spectrum(iq, n):
    """Coarse Welch periodogram, like radio.js, to verify signal energy."""
    import math
    win = [0.5 - 0.5 * math.cos(2 * math.pi * k / (n - 1)) for k in range(n)]
    norm = sum(w * w for w in win) ** 2
    bins = [0.0] * n
    for s in range(max(1, len(iq) // 2 // n)):
        re = [iq[2 * (s * n + k)] * win[k] for k in range(n)]
        im = [iq[2 * (s * n + k) + 1] * win[k] for k in range(n)]
        # Radix-2 FFT in Python
        j = 0
        re2, im2 = re[:], im[:]
        for i in range(1, n):
            bit = n >> 1
            while j & bit:
                j ^= bit
                bit >>= 1
            j |= bit
            if i < j:
                re2[i], re2[j] = re2[j], re2[i]
                im2[i], im2[j] = im2[j], im2[i]
        length = 2
        while length <= n:
            ang = -2 * math.pi / length
            for i in range(0, n, length):
                for k in range(length // 2):
                    wr = math.cos(ang * k)
                    wi = math.sin(ang * k)
                    a = i + k
                    b = a + length // 2
                    tr = re2[b] * wr - im2[b] * wi
                    ti = re2[b] * wi + im2[b] * wr
                    re2[b] = re2[a] - tr
                    im2[b] = im2[a] - ti
                    re2[a] += tr
                    im2[a] += ti
            length <<= 1
        for k in range(n):
            bins[k] += (re2[k] ** 2 + im2[k] ** 2) / norm
    return [10 * math.log10(b) if b > 0 else -120.0 for b in bins]


print(f"\n=== ESP-WebSDR on board ({PORT}) ===\n")

c = Client(PORT)

# Opening the port resets the chip: the USB-JTAG peripheral resets when DTR
# drops while RTS is high (matching resetUsbJtag() in radio.js). Wait for boot
# to complete before talking to the board.
print("[0] boot (opening the port resets the chip)")
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
check("console appears after reset", b"minino>" in boot,
      boot.decode("utf-8", "replace").strip().splitlines()[-1][:50] if boot else "nothing")

# ------------------------------------------------------------- hand over port
print("[0] port hand-over (console sdr_host command)")
c.write(b"sdr_host\r")
banner = ""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 6:
    try:
        chunk = c.ser.read(1)
    except Exception:
        chunk = b""
    if chunk:
        banner += chunk.decode("utf-8", "replace")
        if "comes back" in banner:
            time.sleep(0.3)
            try:
                banner += c.ser.read(4096).decode("utf-8", "replace")
            except Exception:
                pass
            break
check("console responds to sdr_host command", "sdr_host" in banner or "Command" in banner,
      banner.strip().splitlines()[-1][:60] if banner.strip() else "no output")
check("host mode explains how to return", "RELEASE" in banner and "MININO" in banner,
      banner.strip().splitlines()[-1][:60] if banner.strip() else "")
if "Command not found" in banner:
    check("sdr_host command exists", False, "unknown to console")
    sys.exit(1)

# ---------------------------------------------------------------- synchronize
print("\n[1] synchronization (radio.js: synchronize())")
try:
    marker = c.synchronize()
    check("marker echoes back intact", marker.startswith("SYNC "), marker)
except Exception as e:
    check("synchronization", False, str(e))
    sys.exit(1)

# ------------------------------------------------------------------- negotiate
print("\n[2] negotiation (radio.js: negotiate())")
c.command("INFO")
identity = c.line()
m = re.match(r"^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$", identity)
check("INFO matches the identity expected by the viewer", bool(m), identity)
if m:
    n = int(m.group(2))
    check("burst length is between 4096 and 16384", 4096 <= n <= 16384, str(n))
    check("chip family is C6", m.group(1) == "C6", m.group(1))
    MAX_SAMPLES = n
else:
    sys.exit(1)

c.command("CAPS")
caps = c.line()
flags = caps.split(" ")
check("CAPS is non-empty", len(flags) > 1, caps)
for want in ("VERSION", "RXLIMITS", "HWAGC", "IQ8", "SERIALLEASE", "TUNEEXT", "RX40", "RX16",
              "UARTBAUD", "SPEC", "SPECN", "SPECCAPS", "DCT"):
    check(f"CAPS advertises {want}", want in flags)
for absent in ("GPIO", "GAIN", "ALPF", "SPECSTAT"):
    check(f"CAPS does NOT advertise {absent} (viewer would hide that control)", absent not in flags)

if "UARTBAUD" in flags:
    c.command("TRANSPORT?")
    t = re.match(r"^TRANSPORT (UART|USB) (\d+)$", c.line())
    check("TRANSPORT matches expected format", bool(t))
    if t:
        check("transport is USB and baud is 0", t.group(1) == "USB" and t.group(2) == "0",
              f"{t.group(1)} {t.group(2)}")
else:
    c.command("TRANSPORT?")
    t = re.match(r"^TRANSPORT (UART|USB) (\d+)$", c.line())
    check("TRANSPORT matches expected format", bool(t))
    if t:
        check("transport is USB and baud is 0", t.group(1) == "USB" and t.group(2) == "0",
              f"{t.group(1)} {t.group(2)}")

if "TUNEEXT" in flags:
    c.command("RANGE?")
    h = c.line("RANGE ").split(" ")
    lo, hi = int(h[1]), int(h[2])
    valid = (len(h) == 4 and lo >= 100 and lo <= 6000 and lo < hi and h[3] == "1")
    check("RANGE matches expected format", valid, " ".join(h))
    check("range covers Wi-Fi channels", lo <= 2412 and hi >= 2472, f"{lo}..{hi}")
else:
    c.command("RANGE?")
    c.line("RANGE ")

if "RXLIMITS" in flags:
    c.command("LIMITS?")
    limits_line = c.line("LIMITS ")
    try:
        limits = json.loads(limits_line[7:])
        ok = True
    except Exception as e:
        limits = {}
        ok = False
        print("     ", e)
    check("LIMITS returns valid JSON expected by the viewer", ok, limits_line)
    g = limits.get("gain")
    check("gain is [min, max, step]", isinstance(g, list) and len(g) == 3 and
          g[0] >= 0 and g[1] >= g[0] and g[1] <= 127 and g[2] > 0 and
          (g[1] - g[0]) % g[2] == 0, str(g))
    b = limits.get("bandwidth")
    check("bandwidth is null or a valid grid",
          b is None or (isinstance(b, list) and len(b) == 4), str(b))
    rates = limits.get("rates")
    known = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
             16000000, 32000000, 10666667, 6400000]
    check("rates are known to the viewer",
          isinstance(rates, list) and rates and all(r in known for r in rates) and
          len(set(rates)) == len(rates), str(rates))
    bits = limits.get("bits")
    check("bits are 8 and/or 10", isinstance(bits, list) and bits and
          all(x in (8, 10) for x in bits), str(bits))
    RX_RATES = rates
    BITS = bits
else:
    RX_RATES = [80000000]
    BITS = [8, 10]

if "VERSION" in flags:
    c.command("VERSION?")
    line = c.line("", 512)
    ok = line.startswith("VERSION ")
    check("VERSION has the prefix expected by the viewer", ok, line[:60])
    if ok:
        try:
            v = json.loads(line[8:])
            check("VERSION has chip profile", v.get("profile") == "esp32c6",
                  str(v.get("profile")))
            check("VERSION has revision in expected format",
                  isinstance(v.get("revision"), str) and
                  re.match(r"^(?:[a-f0-9]{7,40}(?:-dirty)?|unknown)$", v["revision"]) is not None,
                  str(v.get("revision")))
            check("VERSION has 20-character build_timestamp",
                  len(v.get("build_timestamp", "")) == 20, str(v.get("build_timestamp")))
            check("VERSION build_date matches timestamp prefix",
                  v.get("build_date") == v.get("build_timestamp", "")[:10])
        except Exception as e:
            check("VERSION has valid JSON", False, str(e))
else:
    c.command("VERSION?")

# ------------------------------------------------------------------ tuning
print("\n[3] tuning")
c.command("FREQ 2412")
check("FREQ 2412 replies OK", c.line() == "OK")
c.command("FREQ 2417")
check("FREQ 2417 replies OK (non-Wi-Fi channel center)", c.line() == "OK")
c.command("FREQ 2000")
r = c.line()
check("out-of-range FREQ replies ERR", r.startswith("ERR "), r)
c.command("FREQ 2412")
c.line()

# ------------------------------------------------------------------- captures
print("\n[4] captures")
n = 4096 if QUICK else 16380
for bits in (8, 10):
    try:
        t0 = time.perf_counter()
        data, us = c.capture(n, 80000000, bits)
        dt = time.perf_counter() - t0
        kbps = len(data) / dt / 1000.0
        check(f"CAP{bits*2} of {n} samples", True,
              f"{len(data)} bytes, {us} us capture, {dt*1000:.0f} ms total, "
              f"{kbps:.0f} kB/s")
        iq = decode_iq(data, n, bits)
        peak = max(abs(x) for x in iq)
        rms = (sum(x * x for x in iq) / len(iq)) ** 0.5
        check(f"CAP{bits*2}: values are within range",
              all(-1.01 <= x <= 1.01 for x in iq), f"peak {peak:.3f}")
        check(f"CAP{bits*2}: signal present (not flat noise)", rms > 0.001,
              f"rms {rms:.5f}, peak {peak:.3f}")
    except Exception as e:
        check(f"CAP{bits*2} of {n} samples", False, str(e))

# --------------------------------------------------------------------- rates
print("\n[5] sample rates")
for rate in RX_RATES:
    code = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
            16000000, 32000000, 10666667, 6400000].index(rate)
    c.command(f"CAP16 4096 {code}")
    header = c.line()
    if header.startswith("ERR "):
        check(f"CAP16 at {rate/1e6:g} MS/s", False, header)
        continue
    h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
    if not h:
        check(f"CAP16 at {rate/1e6:g} MS/s", False, header)
        continue
    data = c.read(4096 * 2, time.perf_counter() + 5)
    ok = zlib.crc32(data) == int(h.group(2), 16)
    check(f"CAP16 at {rate/1e6:g} MS/s", ok, f"{int(h.group(3))} us capture")

# ------------------------------------------------------------------- spectrum
if not QUICK:
    print("\n[6] capture spectrum")
    try:
        data, us = c.capture(16380, 80000000, 10)
        iq = decode_iq(data, 16380, 10)
        spec = spectrum(iq, 4096)
        peak = max(spec)
        floor = min(spec)
        print(f"     peak {peak:.1f} dBFS, floor {floor:.1f} dBFS, "
              f"span {peak-floor:.1f} dB")
        check("spectrum has dynamic range", peak - floor > 3.0,
              f"{peak-floor:.1f} dB")
        # 2.4 GHz LO carrier sits at the center of bin 0
        check("peak is at center (LO at 2412 MHz)",
              spec[0] >= max(spec[1:1000]) - 3.0, f"center {spec[0]:.1f} dB")
    except Exception as e:
        check("spectrum", False, str(e))

# ------------------------------------------------------------------ release
print("\n[7] release and escape")
c.command("RELEASE")
check("RELEASE replies OK", c.line() == "OK")

# After RELEASE the session ends and the console returns: the prompt reappears.
c.write(b"\n")
time.sleep(0.5)
back = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 3:
    try:
        d = c.ser.read(4096)
    except Exception:
        d = b""
    if d:
        back += d
    if b"minino>" in back:
        break
check("console returns after RELEASE", b"minino>" in back,
      back.decode("utf-8", "replace").strip().splitlines()[-1][:50] if back else "nothing")

# Minino's own escape command: returns to the console without browser involvement.
c.write(b"sdr_host\r")
time.sleep(0.8)
c.write(b"MININO\n")
time.sleep(0.5)
esc = b""
t0 = time.perf_counter()
while time.perf_counter() - t0 < 4:
    try:
        d = c.ser.read(4096)
    except Exception:
        d = b""
    if d:
        esc += d
    if b"minino>" in esc:
        break
check("MININO returns to the console", b"minino>" in esc,
      esc.decode("utf-8", "replace").strip().splitlines()[-1][:50] if esc else "nothing")

print(f"\n=== {OK} passed, {FAIL} failed ===\n")
sys.exit(1 if FAIL else 0)
