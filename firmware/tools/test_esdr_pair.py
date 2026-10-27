#!/usr/bin/env python3
"""Both SDR applications running simultaneously across two boards.

Only the two SDR applications are used: the native app (I/Q burst with
on-screen UI) and spectrum mode (on-chip FFT). No radiating app is involved:
both boards measure the same RF environment and their results must agree.

Three modes:

  app+spec   native app on the first board and spectrum mode on the second,
              simultaneously
  spec+spec  spectrum mode on both boards simultaneously, comparing the two
              profiles (same environment, same instant)
  app+app    native app on both boards simultaneously

    usage: test_esdr_pair.py <mode> <port_1> <port_2>
"""
import os
import re
import select
import sys
import termios
import threading
import time
import tty
import zlib

# Also allow running this script from another directory.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


class Console:
    """The Minino console: CR-terminated lines, without touching DTR/RTS."""

    def __init__(self, port, baud=115200):
        self.port = port
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

    def wait_console(self, tries=8):
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


# --------------------------------------------------------------------------
# native app
# --------------------------------------------------------------------------

def run_app(port, results, key="app"):
    """Opens the native app, captures bursts, tunes, and exits."""
    log = []
    try:
        con = Console(port)
        time.sleep(0.4)
        con.read(0.6)
        if not con.wait_console():
            results[key] = {"error": "console did not appear"}
            con.close()
            return
        con.write("sdr_capture\r")
        time.sleep(1.0)
        out = con.read(8.0).decode("utf-8", "replace")
        log.append(out)
        bursts = []
        tuned = []
        for _ in range(3):
            con.write("press right\r")
            time.sleep(0.8)
            out = con.read(4.0).decode("utf-8", "replace")
            log.append(out)
            m = re.search(r"burst (\d+): (\d+) words in (\d+) us(.*)", out)
            if m:
                bursts.append((int(m.group(2)), int(m.group(3)), m.group(4).strip()))
            con.write("press up\r")
            time.sleep(0.8)
            out = con.read(3.0).decode("utf-8", "replace")
            log.append(out)
            m = re.search(r"Tuned to (\d+) MHz", out)
            if m:
                tuned.append(int(m.group(1)))
            con.write("press down\r")
            time.sleep(0.8)
            out = con.read(3.0).decode("utf-8", "replace")
            log.append(out)
            m = re.search(r"Tuned to (\d+) MHz", out)
            if m:
                tuned.append(int(m.group(1)))
        con.write("press left\r")
        time.sleep(0.8)
        out = con.read(4.0).decode("utf-8", "replace")
        log.append(out)
        text = "".join(log)
        results[key] = {
            "bursts": bursts,
            "tuned": tuned,
            "entered": "Entering native SDR I/Q capture app" in text,
            "exited": "Exiting native SDR app" in text,
            "radio_free": "free again" in text,
            "sd": "not saved" not in text,
        }
        con.close()
    except Exception as e:  # pragma: no cover
        results["app"] = {"error": str(e)}


# --------------------------------------------------------------------------
# spectrum mode
# --------------------------------------------------------------------------

def run_spec(port, results, key="spec", rate=40000000, bins=1024, seconds=12):
    """Spectrum mode: profiles, frames, CRC, and stop."""
    try:
        con = Console(port)
        time.sleep(0.4)
        con.read(0.6)
        if not con.wait_console():
            results[key] = {"error": "console did not appear"}
            con.close()
            return
        con.write("sdr_host\r")
        time.sleep(1.2)
        banner = con.read(3.0).decode("utf-8", "replace")
        if "comes back" not in banner:
            results[key] = {"error": "host mode did not start"}
            con.close()
            return
        con.write(b"SPECINFO?\n")
        time.sleep(0.4)
        info = con.read(2.0).decode("utf-8", "replace")
        profiles = re.findall(r"\[(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\]", info)
        match = [p for p in profiles if int(p[2]) == bins and int(p[0]) == rate]
        if not match:
            results[key] = {"error": "no profile for requested bins"}
            con.close()
            return
        prof = match[0]
        rate_code, stride, upf = int(prof[1]), int(prof[3]), int(prof[4])
        con.write(f"SPEC 0 {stride} {upf} 0 {rate_code} {bins}\n".encode())
        time.sleep(0.5)
        raw = b""
        end = time.perf_counter() + seconds
        while time.perf_counter() < end:
            try:
                raw += con.read(2.0)
            except Exception:
                pass
        frames = []
        good = 0
        i = 0
        while i + 28 <= len(raw):
            if raw[i:i + 4] != b"SPC1":
                i += 1
                continue
            n = 1 << raw[i + 26]
            total = 28 + n + 4
            if i + total > len(raw):
                break
            body = raw[i:i + 28 + n]
            crc = int.from_bytes(raw[i + 28 + n:i + 32 + n], "little")
            if zlib.crc32(body) != crc:
                i += 1
                continue
            good += 1
            if n == bins:
                frames.append(list(raw[i + 28:i + 28 + n]))
            i += total
        # stop stream: send an empty line
        con.write(b"\n")
        time.sleep(0.8)
        tail = con.read(2.0).decode("utf-8", "replace")
        m = re.search(r"SPECEND ([0-9 ]+)", tail)
        # only frames matching the requested profile enter the average
        frames = [f for f in frames if len(f) == bins]
        acc = [0] * bins
        for f in frames:
            for k, v in enumerate(f):
                acc[k] += v
        avg = [v / len(frames) for v in acc] if frames else []
        results[key] = {
            "frames": len(frames),
            "crc_ok": good,
            "specend": m.group(1).strip() if m else "",
            "profile": avg,
            "bins": bins,
            "rate": rate,
        }
        con.close()
    except Exception as e:  # pragma: no cover
        import traceback
        results[key] = {"error": str(e), "tb": traceback.format_exc()}


def band_profile(profile, bins, rate, mhz_lo, mhz_hi):
    hz = rate / bins
    b0 = int((mhz_lo * 1e6 / hz) % bins)
    b1 = int((mhz_hi * 1e6 / hz) % bins)
    if b1 <= b0:
        return 0.0
    return sum(profile[b0:b1]) / (b1 - b0)


def compare_profiles(a, b, bins, rate):
    """Compares both profiles band by band."""
    if not a or not b:
        return
    print("\n  profile comparison (same environment, same instant):")
    print(f"   {'band':>12} {'board 1':>9} {'board 2':>9} {'diff':>7}")
    worst = 0.0
    for lo in range(-20, 20, 2):
        v1 = band_profile(a, bins, rate, lo, lo + 2)
        v2 = band_profile(b, bins, rate, lo, lo + 2)
        d = v1 - v2
        worst = max(worst, abs(d))
        print(f"   {lo:+6d} MHz {v1:>9.1f} {v2:>9.1f} {d:>+7.1f}")
    print(f"   largest difference between the two boards: {worst:.1f} dB")


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    mode = sys.argv[1]
    p1, p2 = sys.argv[2], sys.argv[3]
    results = {}

    print(f"\n=== {mode}: {p1} and {p2} ===\n")

    if mode == "app+spec":
        t1 = threading.Thread(target=run_app, args=(p1, results))
        t2 = threading.Thread(target=run_spec, args=(p2, results))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
    elif mode == "spec+spec":
        t1 = threading.Thread(target=run_spec, args=(p1, results, "spec1"))
        t2 = threading.Thread(target=run_spec, args=(p2, results, "spec2"))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
    elif mode == "app+app":
        t1 = threading.Thread(target=run_app, args=(p1, results, "app1"))
        t2 = threading.Thread(target=run_app, args=(p2, results, "app2"))
        t1.start()
        t2.start()
        t1.join()
        t2.join()
    else:
        print("unknown mode")
        sys.exit(1)

    print("\n--- native app ---")
    print("  (keys:", sorted(results.keys()), ")")
    for key in ("app", "app1", "app2"):
        if key not in results:
            continue
        r = results[key]
        print(f"  {key}: {r}")
    print("\n--- spectrum mode ---")
    for key in ("spec", "spec1", "spec2"):
        if key not in results:
            continue
        r = results[key]
        if r.get("error"):
            print(f"  {key}: ERROR {r['error']}")
            if r.get("tb"):
                print(r["tb"])
            continue
        prof = r.get("profile") or []
        print(f"  {key}: {r['frames']} frames with valid CRC, "
              f"bins={r['bins']}, SPECEND [{r['specend']}]")
        if prof:
            print(f"    peak {max(prof):.0f}  median {sorted(prof)[len(prof)//2]:.0f}  "
                  f"min {min(prof):.0f}")
    if mode == "spec+spec" and results.get("spec1") and results.get("spec2"):
        compare_profiles(results["spec1"].get("profile"),
                         results["spec2"].get("profile"),
                         results["spec1"]["bins"], results["spec1"]["rate"])


if __name__ == "__main__":
    main()
