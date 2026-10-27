#!/usr/bin/env python3
"""Over-the-air test: one board transmits and the other receives with the SDR.

The receiving board enters host mode (the same protocol spoken by the browser
viewer) and measures; the transmitting board alternates between an active
radiating app (BLE spammer or Wi-Fi beacon spammer) and idle.

Two measurements, depending on mode:

  spec  The board computes the FFT (SPEC mode) and compares the mean spectrum
        with transmission active against windows without transmission. This is
        what the browser viewer sees.

  iq    Requests I/Q bursts (CAP20) and measures narrowband power within a
        1 MHz window around the target frequency. A narrow signal (a BLE
        advertisement is 1 MHz wide) appears as a peak; surrounding Wi-Fi
        traffic spans 11 MHz and spreads its energy, allowing the narrow
        window to distinguish them.

    usage:  python3 tools/test_esdr_radio.py <rx_port> <tx_port> [options]

Options:
    --mode M     "spec" (default) or "iq"
    --on CMD     console command that starts transmission (default
                 "launch spam_all"). Multiple commands separated by ';' are
                 sent in order (the beacon spammer needs to open the menu and
                 press Start: "launch ssid_spam;press down;press right")
    --off CMD    command that stops it (default "exit_app")
    --rate HZ    spectrum sample rate (80000000, 40000000, 16000000)
    --bins N     FFT bins (256, 512, 1024, 2048)
    --seconds N  measurement duration in seconds
    --period N   seconds per state window
    --freq HZ    tuning frequency on the receiving board
    --freqs HZ   frequencies (in MHz) measured in iq mode, separated by spaces
                 (default: BLE advertising channels plus reference frequencies)

Requires pyserial.
"""
import math
import re
import sys
import threading
import time
import zlib

import serial


def arg_value(flag, default):
    if flag in sys.argv:
        return sys.argv[sys.argv.index(flag) + 1]
    return default


class Console:
    """The Minino console: lines are terminated with CR, not LF."""

    def __init__(self, port):
        self.port = port
        self.raw = serial.Serial(port, 115200, timeout=5)
        self.raw.dtr = False
        self.raw.rts = False

    def send(self, text):
        """Sends a line. Multiple commands separated by ';' are sent in order."""
        for part in text.split(";"):
            part = part.strip()
            if not part:
                continue
            self.raw.write((part + "\r").encode())
            self.raw.flush()
            time.sleep(0.4)

    def drain(self, seconds=0.5):
        out = b""
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            try:
                d = self.raw.read(4096)
            except Exception:
                break
            if d:
                out += d
        return out


class TxToggle(threading.Thread):
    """Toggles transmission on and off on the second board.

    Each state change opens a new window: windows are numbered so each
    transmission window can be compared against its neighboring idle windows
    (the RF environment drifts over time, and comparing against adjacent
    windows cancels that drift out).
    """

    daemon = True

    def __init__(self, console, period, on_cmd, off_cmd):
        super().__init__()
        self.console = console
        self.period = period
        self.on_cmd = on_cmd
        self.off_cmd = off_cmd
        self.state = True
        self.window = 0
        self.lock = threading.Lock()

    def state_now(self):
        with self.lock:
            return self.state

    def window_now(self):
        with self.lock:
            return self.window

    def run(self):
        next_switch = time.perf_counter() + self.period
        while True:
            now = time.perf_counter()
            if now >= next_switch:
                with self.lock:
                    self.state = not self.state
                    self.window += 1
                    on = self.state
                self.console.send(self.on_cmd if on else self.off_cmd)
                time.sleep(0.6)
                self.console.drain(0.3)
                next_switch = now + self.period
            time.sleep(0.05)


def read_line(ser, timeout=10.0):
    s = ""
    end = time.perf_counter() + timeout
    while time.perf_counter() < end:
        try:
            b = ser.read(1)
        except Exception:
            b = b""
        if not b:
            continue
        if b[0] == 10:
            return s.strip()
        s += chr(b[0])
    raise TimeoutError("no response received")


def open_host_session(port):
    """Hands over the port using the console's sdr_host command."""
    ser = serial.Serial(port, 115200, timeout=5)
    ser.dtr = False
    ser.rts = False
    boot = b""
    end = time.perf_counter() + 8
    while time.perf_counter() < end:
        try:
            d = ser.read(4096)
        except Exception:
            d = b""
        if d:
            boot += d
            if b"minino>" in boot:
                break
    if b"minino>" not in boot:
        print("console did not appear on the RX port")
        sys.exit(1)

    banner = b""
    for _ in range(3):
        ser.write(b"sdr_host\r")
        time.sleep(1.2)
        end = time.perf_counter() + 5
        while time.perf_counter() < end:
            try:
                d = ser.read(4096)
            except Exception:
                d = b""
            if d:
                banner += d
                if b"comes back" in banner:
                    break
        if b"comes back" in banner:
            break
        ser.write(b"MININO\r")
        time.sleep(1.0)
    if b"comes back" not in banner:
        print("host mode did not start:", banner[-200:])
        sys.exit(1)
    print("host mode ready")
    return ser


def decode_iq20(data, n):
    """CAP20: 20 bits per pair, I in bits 0-9 and Q in bits 10-19."""
    acc = 0
    held = 0
    k = 0
    il = []
    ql = []
    for _ in range(n):
        while held < 20:
            acc |= data[k] << held
            k += 1
            held += 8
        word = acc & 0xFFFFF
        acc >>= 20
        held -= 20
        i = word & 1023
        q = (word >> 10) & 1023
        il.append((i - 1024 if i >= 512 else i) / 512.0)
        ql.append((q - 1024 if q >= 512 else q) / 512.0)
    return il, ql


def fft_mag(re, im):
    """In-place radix-2 FFT: inputs are real and imaginary arrays."""
    n = len(re)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            re[i], re[j] = re[j], re[i]
            im[i], im[j] = im[j], im[i]
    length = 2
    while length <= n:
        ang = -2 * math.pi / length
        half = length // 2
        for i in range(0, n, length):
            for k in range(half):
                wr = math.cos(ang * k)
                wi = math.sin(ang * k)
                a = i + k
                b = a + half
                tr = re[b] * wr - im[b] * wi
                ti = re[b] * wi + im[b] * wr
                re[b] = re[a] - tr
                im[b] = im[a] - ti
                re[a] += tr
                im[a] += ti
        length <<= 1


def capture_iq(ser, samples):
    """Captures one I/Q burst in 20-bit-per-pair format."""
    header = ""
    for attempt in range(4):
        ser.write(f"CAP20 {samples} 0\n".encode())
        header = read_line(ser, 15.0)
        if header.startswith("DATA "):
            break
        # port may have leftover bytes: keep reading until the actual header
        print("    (unexpected line:", header[:40], ")")
    if header.startswith("ERR "):
        raise ValueError(header)
    h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
    if not h:
        raise ValueError("invalid header: " + header)
    want = -(-(samples * 20) // 8)
    data = b""
    end = time.perf_counter() + 10
    while len(data) < want and time.perf_counter() < end:
        chunk = ser.read(min(4096, want - len(data)))
        if chunk:
            data += chunk
    if zlib.crc32(data) != int(h.group(2), 16):
        # a burst may arrive truncated: retry
        if attempt + 1 < 4:
            return capture_iq(ser, samples)
        raise ValueError("CRC mismatch")
    return decode_iq20(data, samples)


def mode_iq(ser, toggle, freqs_hz, lo_hz, samples, rate, seconds):
    """Measures narrowband power at several frequencies, with and without TX.

    On each burst, power is measured in a narrow window around each requested
    frequency. A narrowband signal (a BLE advertisement is 1 MHz wide) only
    shows up in its channel; ambient traffic (11 MHz wide) spreads its energy
    across all windows.
    """
    on = []
    off = []
    # radix-2 FFT requires a power of two: zero-pad the burst
    n = 1
    while n < samples:
        n <<= 1
    bin_hz = rate / n
    half = max(1, int(500e3 / bin_hz))
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        il, ql = capture_iq(ser, samples)
        re = list(il) + [0.0] * (n - len(il))
        im = [-v for v in ql] + [0.0] * (n - len(ql))
        fft_mag(re, im)

        def window(freq):
            # offset may be negative: the FFT spans -rate/2 to +rate/2,
            # so the bin is the signed offset modulo n
            k = int((freq - lo_hz) / bin_hz) % n
            p = 0.0
            for j in range(max(0, k - half), min(n, k + half + 1)):
                p += re[j] ** 2 + im[j] ** 2
            return 10 * math.log10(p / n + 1e-20)

        row = [window(f) for f in freqs_hz]
        (on if toggle.state_now() else off).append(row)
        time.sleep(0.05)

    print(f"\n  power in {2 * half * bin_hz / 1e6:.2f} MHz windows "
          f"(LO {lo_hz / 1e6:g} MHz), peak across bursts:")
    print(f"   {'frequency':>12} {'TX ON':>9} {'TX OFF':>9} {'diff':>8}   "
          f"{'p90 ON':>9} {'p90 OFF':>9} {'diff':>8}")
    for i, f in enumerate(freqs_hz):
        a = sorted(r[i] for r in on)
        b = sorted(r[i] for r in off)
        if not a or not b:
            continue
        p90a = a[int(len(a) * 0.9)]
        p90b = b[int(len(b) * 0.9)]
        print(f"   {f / 1e6:>9.1f} MHz {a[-1]:>9.2f} {b[-1]:>9.2f} "
              f"{a[-1] - b[-1]:>+8.2f}   {p90a:>9.2f} {p90b:>9.2f} {p90a - p90b:>+8.2f}")
    print(f"\n  bursts: TX ON {len(on)}, TX OFF {len(off)}")


def mode_spec(ser, toggle, rate, bins, seconds):
    """On-chip spectrum (SPEC mode) with and without transmission."""
    ser.write(b"SPECINFO?\n")
    time.sleep(0.4)
    info = b""
    end = time.perf_counter() + 5
    while time.perf_counter() < end:
        d = ser.read(4096)
        info += d
        if b"\n" in info:
            break
    profiles = re.findall(r"\[(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\]", info.decode())
    match = [p for p in profiles if int(p[2]) == bins and int(p[0]) == rate]
    if not match:
        print(f"no profile for {bins} bins at {rate / 1e6:g} MS/s")
        sys.exit(1)
    rate_code, stride, upf = int(match[0][1]), int(match[0][3]), int(match[0][4])
    print(f"profile: {match[0]}")

    ser.write(f"SPEC 0 {stride} {upf} 0 {rate_code} {bins}\n".encode())
    time.sleep(0.5)

    windows = {}
    pending = b""
    end = time.perf_counter() + seconds
    while time.perf_counter() < end:
        try:
            chunk = ser.read(65536)
        except Exception:
            chunk = b""
        if not chunk:
            continue
        pending += chunk
        while True:
            k = pending.find(b"SPC1")
            if k < 0:
                pending = pending[-3:]
                break
            if k:
                pending = pending[k:]
            if len(pending) < 32:
                break
            n = 1 << pending[26]
            total = 28 + n + 4
            if len(pending) < total:
                break
            body = pending[:28 + n]
            crc = int.from_bytes(pending[28 + n:32 + n], "little")
            if zlib.crc32(body) != crc:
                pending = pending[1:]
                continue
            frame = pending[28:28 + n]
            pending = pending[total:]
            key = (toggle.window_now(), toggle.state_now())
            windows.setdefault(key, []).append(frame)

    on = [f for (w, st), frames in windows.items() if st for f in frames]
    off = [f for (w, st), frames in windows.items() if not st for f in frames]
    print(f"\nframes: TX ON {len(on)} ({len([1 for k in windows if k[1]])} windows), "
          f"TX OFF {len(off)} ({len([1 for k in windows if not k[1]])} windows)")
    if not on or not off:
        print("not enough frames to compare")
        sys.exit(1)

    def mean(frames):
        total = [0] * bins
        for f in frames:
            for i, v in enumerate(f):
                total[i] += v
        return [v / len(frames) for v in total]

    m_on = mean(on)
    m_off = mean(off)
    diff = [a - b for a, b in zip(m_on, m_off)]

    # Paired comparison: each transmission window is compared against its
    # adjacent idle windows. The RF environment drifts over time, and pairing
    # with neighboring windows cancels that drift out, leaving the transmitter's
    # signal.
    paired = []
    keys = sorted(windows.keys())
    for i, (w, st) in enumerate(keys):
        if not st or not windows[(w, st)]:
            continue
        neigh = []
        for j in (i - 1, i + 1):
            if 0 <= j < len(keys) and not keys[j][1] and windows[keys[j]]:
                neigh.append(mean(windows[keys[j]]))
        if not neigh:
            continue
        base = [sum(v) / len(neigh) for v in zip(*neigh)]
        cur = mean(windows[(w, st)])
        paired.append([a - b for a, b in zip(cur, base)])
    if paired:
        diff_paired = [sum(v) / len(paired) for v in zip(*paired)]
        print(f"  paired comparison: {len(paired)} TX windows against their neighbors")
    else:
        diff_paired = diff

    hz = rate / bins
    print(f"  TX ON : peak {max(m_on):.0f}  median {sorted(m_on)[bins // 2]:.0f}  "
          f"sum {sum(m_on):.0f}")
    print(f"  TX OFF: peak {max(m_off):.0f}  median {sorted(m_off)[bins // 2]:.0f}  "
          f"sum {sum(m_off):.0f}")

    print("\n  top ten bins by difference (TX ON - TX OFF, paired):")
    for t in sorted(range(bins), key=lambda k: -diff_paired[k])[:10]:
        print(f"   {t * hz / 1e6:+8.3f} MHz from LO   diff {diff_paired[t]:+6.1f}")

    span = int(rate / 2 / 1e6)
    print("\n  difference per 1 MHz band (paired):")
    for i in range(-span, span):
        lo, hi = i * 1e6, (i + 1) * 1e6
        b0 = int((lo / hz) % bins)
        b1 = int((hi / hz) % bins)
        if b1 <= b0:
            continue
        d = sum(diff_paired[b0:b1]) / (b1 - b0)
        print(f"   {i:+4d} MHz  {d:+6.2f} {'#' * max(0, int((d + 5) * 2))}")


def main():
    if len(sys.argv) < 3 or sys.argv[1].startswith("--"):
        print(__doc__)
        sys.exit(1)
    rx_port, tx_port = sys.argv[1], sys.argv[2]
    on_cmd = arg_value("--on", "launch spam_all")
    off_cmd = arg_value("--off", "exit_app")
    mode = arg_value("--mode", "spec")
    rate = int(arg_value("--rate", 40000000))
    bins = int(arg_value("--bins", 1024))
    seconds = float(arg_value("--seconds", 24))
    period = float(arg_value("--period", 3.0))
    freq = int(arg_value("--freq", 2412))
    freqs = [int(x) * 1000000 for x in
             arg_value("--freqs", "2402 2407 2412 2417 2422 2426 2432").split()]
    samples = int(arg_value("--samples", 16380))

    print(f"=== Over-the-air test: RX {rx_port}, TX {tx_port} ===\n")
    print(f"  mode: {mode}")
    print(f"  transmission: '{on_cmd}' / '{off_cmd}'")
    if mode == "spec":
        print(f"  spectrum: {rate / 1e6:g} MS/s, {bins} bins, {seconds:g} s")
    else:
        print(f"  I/Q: {samples} samples at {rate / 1e6:g} MS/s, "
              f"LO {freq} MHz, frequencies {[f // 1000000 for f in freqs]} MHz, "
              f"{seconds:g} s")

    tx = Console(tx_port)
    tx.drain(0.5)

    ser = open_host_session(rx_port)
    ser.write(f"FREQ {freq}\n".encode())
    print("tuning:", read_line(ser, 10.0))

    toggle = TxToggle(tx, period, on_cmd, off_cmd)
    toggle.start()
    time.sleep(0.5)

    try:
        if mode == "iq":
            mode_iq(ser, toggle, freqs, freq, samples, rate, seconds)
        else:
            mode_spec(ser, toggle, rate, bins, seconds)
    finally:
        tx.send(off_cmd)
        time.sleep(1.0)
        tx.drain(0.3)
        ser.close()


if __name__ == "__main__":
    main()
