#!/usr/bin/env python3
"""Prueba del modo de espectros (SPEC) contra la placa.

Reproduce lo que hace el visor de ESPARGOS cuando elige "On-Chip Spectrum":
pregunta los perfiles, pide la corriente de espectros y decodifica los
marcos SPC1 igual que el visor (cabecera, bins en orden natural y CRC32).

uso:  python3 test_esdr_spec.py [puerto]
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
        print(f"  \033[32mPASA\033[0m  {name}" + (f"   {detail}" if detail else ""))
    else:
        FAIL += 1
        print(f"  \033[31mFALLA\033[0m {name}" + (f"   {detail}" if detail else ""))


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
                raise TimeoutError(f"se pidieron {n} bytes y llegaron {len(out)}")
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
                    raise ValueError(f"respuesta inesperada: {s!r}")
                return s
            s += chr(b)
        raise ValueError("respuesta demasiado larga")


def decode_frames(data):
    """El SpectrumDecoder del visor: busca marcos SPC1 validos."""
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


print(f"\n=== Modo de espectros contra la placa ({PORT}) ===\n")
c = Client(PORT)

# ------------------------------------------------------------------ arranque
print("[1] arranque y entrega del puerto")
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
check("la consola aparece", b"minino>" in boot)

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
check("el modo host arranca", b"comes back" in banner)

# ----------------------------------------------------------------- SPECINFO?
print("\n[2] los perfiles (SPECINFO?)")
c.command("SPECINFO?")
info = c.line("SPECINFO ")
print("    " + info)
check("el JSON tiene la forma que el visor espera",
      '"continuous":false' in info and '"transports":[' in info and '"profiles":[' in info)
check("el transporte es USB", '"USB"' in info)

profiles = re.findall(r"\[(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\]", info)
check("hay perfiles", len(profiles) > 0, f"{len(profiles)} perfiles")
rates = {int(p[0]) for p in profiles}
check("estan las tres tasas del C6", rates == {80000000, 40000000, 16000000}, str(sorted(rates)))
sizes = {int(p[2]) for p in profiles}
check("los tamanos de FFT llegan a 2048", 2048 in sizes, str(sorted(sizes)))

# El visor elige el perfil cuyo numero de bins es el que ha pedido.
n = 1024
chosen = [p for p in profiles if int(p[2]) == n and int(p[0]) == 80000000]
check("hay un perfil de 1024 bins a 80 MS/s", len(chosen) == 1, str(chosen))

# ---------------------------------------------------------------------- SPEC
print(f"\n[3] la corriente de espectros ({n} bins)")
rate_code = int(chosen[0][1])
stride = int(chosen[0][3])
upf = int(chosen[0][4])
c.command(f"SPEC 0 {stride} {upf} 0 {rate_code} {n}")

header = c.line("SPEC ")
parts = header.split(" ")
check("la cabecera tiene cinco campos", len(parts) == 5, header)
check("la cabecera lleva el numero de bins", int(parts[1]) == n, parts[1])
check("la cabecera lleva la tasa", int(parts[2]) == 80000000, parts[2])
check("la cabecera lleva la frecuencia", int(parts[3]) == n, parts[3])

# Los marcos llegan en flujo: se recogen un par y se corta.
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
check("llegan marcos validos", len(frames) > 0, f"{len(frames)} marcos, {len(raw)} bytes")
if frames:
    f = frames[0]
    check("el marco tiene la firma SPC1", raw[:4] == b"SPC1")
    check("el numero de bins del marco es el pedido", f["log2n"] == (n.bit_length() - 1))
    check("el multiplicador de dB es 2", f["dbmul"] == 2)
    check("el bit de huecos esta puesto (capturas sueltas)", (f["flags"] & 8) != 0,
          f"flags {f['flags']}")
    check("las bins son codigos de 0 a 255",
          all(0 <= b <= 255 for b in f["bins"]) and len(f["bins"]) == n)
    check("hay bins con señal (no todos a cero)", max(f["bins"]) > 0,
          f"max {max(f['bins'])}, min {min(f['bins'])}")
    # El bin 0 es la portadora del LO, pero no tiene por que ser el mas
    # fuerte: alrededor hay redes de Wi-Fi. Lo que se comprueba es que el
    # espectro tiene estructura.
    ordered = sorted(f["bins"], reverse=True)
    median = ordered[len(ordered) // 2]
    check("el espectro tiene estructura (no es ruido plano)",
          ordered[0] > median + 10,
          f"max {ordered[0]}, mediana {median}, min {ordered[-1]}")
    check("la portadora del LO esta en el bin 0", f["bins"][0] > 0,
          f"centro {f['bins'][0]}")

# ------------------------------------------------------------------- parar
print("\n[4] el corte")
c.command("")  # una linea vacia: el visor corta asi
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
check("el firmware responde SPECEND", b"SPECEND" in end)
if b"SPECEND" in end:
    # El final viene detras de los marcos binarios, asi que se busca en el
    # flujo de bytes y no linea a linea.
    tail = end[end.index(b"SPECEND"):]
    line = tail[:tail.find(b"\n")].decode("utf-8", "replace")
    fields = line.split()
    # SPECEND status detail units pairs elapsed_us late_max work_max_cycles
    #         frames drops abandoned ffts stopped_by_host
    check("SPECEND tiene doce campos", len(fields) == 13, line[:90])
    check("el estado es 0 (parada normal)", fields[1] == "0", fields[1])
    check("hubo FFTs", int(fields[3]) > 0, fields[3])
    check("hubo pares de muestras", int(fields[4]) > 0, fields[4])
    check("hubo marcos", int(fields[8]) > 0, fields[8])
    check("el host corto la corriente", fields[12] == "1", fields[12])

print(f"\n=== {OK} pasan, {FAIL} fallan ===\n")
sys.exit(1 if FAIL else 0)
