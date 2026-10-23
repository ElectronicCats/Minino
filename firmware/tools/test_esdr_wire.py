#!/usr/bin/env python3
"""El cable, byte a byte: lo que la placa envia y lo que recibe.

Solo se usan las dos aplicaciones del SDR: el modo host (que es por donde
llega el modo de espectros) y la rafaga. El script saca el intercambio
real del puerto y lo enseña en hexadecimal, para ver el formato de cada
paquete.

    uso: test_esdr_wire.py <puerto>
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
    """El puerto tal cual, sin tocar DTR/RTS (hay pilas que lo coaster)."""

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
        """Lee una linea. Devuelve la linea y lo que queda en el puerto: la
        carga binaria va justo detras del LF y no se puede perder."""
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
        """Espera al prompt. Si la placa se quedo en el modo host, una
        linea vacia y RELEASE la sacan de ahi."""
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
        lines.append(f"   ... ({len(data)} bytes en total)")
    return "\n".join(lines)


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
    w = Wire(port)
    time.sleep(0.5)
    w.read(1.0)

    print(f"\n=== El cable de la placa ({port}) ===\n")

    # ---------------------------------------------------------- la entrega
    if not w.wait_console():
        print("la consola no aparece")
        sys.exit(1)
    banner = ""
    for _ in range(3):
        w.write("sdr_host\r")
        time.sleep(1.5)
        banner = w.read(3.0).decode("utf-8", "replace")
        if "comes back" in banner:
            break
    if "comes back" not in banner:
        print("el modo host no arranca:", banner[-200:])
        sys.exit(1)
    print("[1] la entrega del puerto (la consola se queda muda)")
    for line in banner.splitlines():
        if line.strip():
            print("   " + line)
    print("   (todo lo que viene de la placa a partir de aqui va sin logs:")
    print("    el nivel maestro baja a ESP_LOG_NONE durante la sesion)")

    # ------------------------------------------------------- el handshake
    print("\n[2] el handshake (texto)")
    for cmd in ("SYNC 1792629170881", "INFO", "CAPS", "RANGE?", "FREQ 2412"):
        w.write(cmd + "\n")
        time.sleep(0.3)
        line, _ = w.read_line(5.0)
        print(f"   -> {cmd}")
        print(f"   <- {line}")

    # ------------------------------------------------------- la rafaga
    print("\n[3] la rafaga (CAP20): la cabecera y el comienzo de la carga")
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
        print(f"   la carga son {len(data)} bytes para {n} muestras "
              f"(20 bits por par, I en los bits 0-9 y Q en los 10-19)")
        print(f"   el CRC32 (zlib) de la carga: "
              f"{zlib.crc32(data):08x}  (el que anuncia la cabecera: {m.group(2)})")
        print("   los primeros bytes de la carga:")
        print(hexdump(data))

    # ------------------------------------------------------- el espectro
    print("\n[4] el modo de espectros (SPEC): la cabecera y un marco SPC1")
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
    # un marco entero
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
        print(f"   el marco SPC1: {total} bytes ({bins} bins)")
        print("   la cabecera (28 bytes):")
        print(hexdump(frame, 0, 28))
        print("   las primeras bins:")
        print(hexdump(frame[28:], 28, 32))
        print("   el CRC32 de la cabecera y las bins: "
              f"{zlib.crc32(frame[:-4]):08x}  "
              f"(el que lleva el marco: {int.from_bytes(frame[-4:], 'little'):08x})")
    else:
        print("   no se ha podido separar un marco del flujo")

    # ------------------------------------------------------- el corte
    print("\n[5] el corte (una linea vacia)")
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
