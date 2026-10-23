#!/usr/bin/env python3
"""Prueba de la app nativa del SDR (Applications > SDR / sdr_capture).

Solo se usa la app: se abre, se capturan rafagas, se sintoniza y se sale.
Las comprobaciones se hacen sobre lo que dice el log de la placa.

    uso: sdr_app_test.py <puerto>
"""
import os
import re
import select
import sys
import termios
import time
import tty


class Console:
    """La consola de Minino.

    Se habla con la placa sin tocar DTR/RTS: hay pilas de ESP32-C6 cuyo
    USB se queda en EP0 y el pyserial aborta al abrir el puerto.  Ademas
    la consola cierra las lineas con CR, no con LF."""

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
        """Manda una linea a la consola y devuelve lo que contest."""
        self.write(text + "\r")
        time.sleep(0.4)
        return self.read(seconds).decode("utf-8", "replace")

    def wait_console(self, tries=8):
        """Espera al prompt.

        La placa puede quedarse en el modo host (o con una corriente de
        espectros abierta): una linea vacia corta la corriente y RELEASE
        cierra la sesion, y la consola vuelve."""
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
        print(f"  \033[32mPASA\033[0m  {name}" + (f"   {detail}" if detail else ""))
    else:
        FAIL += 1
        print(f"  \033[31mFALLA\033[0m {name}" + (f"   {detail}" if detail else ""))


def main():
    print(f"\n=== App nativa del SDR contra la placa ({PORT}) ===\n")
    r = Console(PORT)
    time.sleep(0.5)
    r.read(1.0)
    if not r.wait_console():
        print("la consola no aparece")
        sys.exit(1)

    # ------------------------------------------------------------- abrir la app
    print("[1] abrir la app")
    r.write(b"sdr_capture\r")
    time.sleep(1.0)
    out = r.read(8.0).decode("utf-8", "replace")
    print("    " + "\n    ".join(l for l in out.splitlines() if l.strip())[:1600])
    check("la app arranca", "Entering native SDR I/Q capture app" in out)
    check("el motor de captura se levanta",
          "Initializing C6 native SDR RX engine" in out)
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("sintoniza al entrar", m is not None, m.group(0) if m else "")
    sd_ok = "microSD not available" not in out
    check("la microSD responde", sd_ok,
          "la tarjeta esta presente" if sd_ok else "no hay tarjeta o no contesta")

    # ------------------------------------------------------------- la rafaga
    print("\n[2] la ráfaga (RIGHT)")
    r.write(b"press right\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    m = re.search(r"burst (\d+): (\d+) words in (\d+) us(.*)", out)
    check("hay ráfaga", m is not None, m.group(0) if m else out[-200:])
    if m:
        words, us = int(m.group(2)), int(m.group(3))
        check("la rafaga son 16380 palabras", words == 16380, str(words))
        rate = words / us  # palabras por microsegundo = MS/s
        check("la tasa esta cerca de los 80 MS/s", 60 < rate < 90,
              f"{rate:.1f} MS/s")
        check("la rafaga se ha guardado en la microSD", "not saved" not in m.group(4),
              m.group(4).strip() or "guardada")

    # -------------------------------------------------------------- sintonía
    print("\n[3] la sintonía (UP y DOWN)")
    r.write(b"press up\r")
    time.sleep(2.0)
    out = r.read(2.0).decode("utf-8", "replace")
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("UP sintoniza 5 MHz mas", m is not None and m.group(1) == "2417",
          m.group(0) if m else out[-160:])
    r.write(b"press down\r")
    time.sleep(2.0)
    out = r.read(2.0).decode("utf-8", "replace")
    m = re.search(r"Tuned to (\d+) MHz \(CH (\d+)\)", out)
    check("DOWN vuelve al canal 1", m is not None and m.group(1) == "2412",
          m.group(0) if m else out[-160:])

    # ------------------------------------------------------------- segunda
    print("\n[4] una segunda ráfaga")
    r.write(b"press right\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    m = re.search(r"burst (\d+): (\d+) words in (\d+) us", out)
    check("la segunda rafaga tambien", m is not None, m.group(0) if m else out[-200:])
    if m:
        check("el contador de rafagas avanza", int(m.group(1)) >= 2, m.group(1))

    # ---------------------------------------------------------------- salida
    print("\n[5] la salida (LEFT)")
    r.write(b"press left\r")
    time.sleep(1.0)
    out = r.read(4.0).decode("utf-8", "replace")
    check("la app sale", "Exiting native SDR app" in out)
    check("el motor de captura se para", "Stopping C6 native SDR RX engine" in out)
    check("la radio vuelve a Minino",
          "Wi-Fi driver was started for the capture and is free again" in out)

    # la consola sigue viva
    r.write(b"display\r")
    time.sleep(1.5)
    out = r.read(1.5).decode("utf-8", "replace")
    check("la consola sigue viva", "minino>" in out or "in_app" in out,
          out.strip().splitlines()[-1][:60] if out.strip() else "nada")

    print(f"\n=== {OK} pasan, {FAIL} fallan ===\n")
    r.close()
    sys.exit(1 if FAIL else 0)


if __name__ == "__main__":
    main()
