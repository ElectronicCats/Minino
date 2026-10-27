#!/usr/bin/env python3
"""Test for the native SDR app (Applications > SDR / sdr_capture).

Only uses the app: opens it, captures bursts, tunes, and exits.
Checks are performed against the board's log output.

    usage: sdr_app_test.py <port>
"""
import os
import re
import select
import sys
import termios
import time
import tty


class Console:
    """The Minino console.

    Communicates with the board without touching DTR/RTS: some ESP32-C6 stacks
    leave USB in EP0 and pyserial aborts when opening the port. Additionally,
    the console terminates lines with CR rather than LF."""

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

    def command(self, text, seconds=2.0):
        """Sends a line to the console and returns the reply."""
        self.write(text + "\r")
        time.sleep(0.4)
        return self.read(seconds).decode("utf-8", "replace")

    def wait_console(self, tries=8):
        """Waits for the prompt.

        The board may still be in host mode (or have an active spectrum
        stream): an empty line stops the stream and RELEASE closes the
        session, bringing the console back."""
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


PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

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


def main():
    print(f"\n=== Native SDR app on board ({PORT}) ===\n")
    r = Console(PORT)
    time.sleep(0.5)
    r.read(1.0)
    if not r.wait_console():
        print("console did not appear")
        sys.exit(1)

    # ------------------------------------------------------------- open app
    print("[1] open app")
    r.write(b"sdr_capture\r")
    time.sleep(1.0)
    out = r.read(8.0).decode("utf-8", "replace")
    print("    " + "\n    ".join(l for l in out.splitlines() if l.strip())[:1600])
    check("app starts", "Entering native SDR I/Q capture app" in out)
    check("capture engine starts",
          "Initializing C6 native SDR RX engine" in out)
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("tunes on entry", m is not None, m.group(0) if m else "")
    sd_ok = "microSD not available" not in out
    check("microSD responds", sd_ok,
          "card is present" if sd_ok else "card missing or not responding")

    # ------------------------------------------------------------- burst
    print("\n[2] burst capture (RIGHT)")
    r.write(b"press right\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    m = re.search(r"burst (\d+): (\d+) words in (\d+) us(.*)", out)
    check("burst captured", m is not None, m.group(0) if m else out[-200:])
    if m:
        words, us = int(m.group(2)), int(m.group(3))
        check("burst is 16380 words", words == 16380, str(words))
        rate = words / us  # words per microsecond = MS/s
        check("rate is near 80 MS/s", 60 < rate < 90,
              f"{rate:.1f} MS/s")
        check("burst saved to microSD", "not saved" not in m.group(4),
              m.group(4).strip() or "saved")

    # -------------------------------------------------------------- tuning
    print("\n[3] tuning (UP and DOWN)")
    r.write(b"press up\r")
    time.sleep(2.0)
    out = r.read(2.0).decode("utf-8", "replace")
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("UP tunes +5 MHz", m is not None and m.group(1) == "2417",
          m.group(0) if m else out[-160:])
    r.write(b"press down\r")
    time.sleep(2.0)
    out = r.read(2.0).decode("utf-8", "replace")
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("DOWN returns to channel 1", m is not None and m.group(1) == "2412",
          m.group(0) if m else out[-160:])

    # ------------------------------------------------------------- second burst
    print("\n[4] second burst")
    r.write(b"press right\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    m = re.search(r"burst (\d+): (\d+) words in (\d+) us", out)
    check("second burst succeeds", m is not None, m.group(0) if m else out[-200:])
    if m:
        check("burst counter increments", int(m.group(1)) >= 2, m.group(1))

    # ---------------------------------------------------------------- exit
    print("\n[5] exit (LEFT)")
    r.write(b"press left\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    check("app exits", "Exiting native SDR app" in out)
    check("capture engine stops", "Stopping C6 native SDR RX engine" in out)
    check("radio returns to Minino",
          "Wi-Fi driver was started for the capture and is free again" in out)

    # console remains alive
    r.write(b"display\r")
    time.sleep(1.5)
    out = r.read(1.5).decode("utf-8", "replace")
    check("console remains responsive", "minino>" in out or "in_app" in out,
          out.strip().splitlines()[-1][:60] if out.strip() else "nothing")

    print(f"\n=== {OK} passed, {FAIL} failed ===\n")
    r.close()
    sys.exit(1 if FAIL else 0)


if __name__ == "__main__":
    main()
