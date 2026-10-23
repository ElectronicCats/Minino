#!/usr/bin/env python3
"""Prueba de la placa contra el protocolo del navegador de ESPARGOS.

Este script replica la logica de esp-web-sdr (radio.js) lo mas fielmente
posible: el mismo handshake, las mismas expresiones regulares, la misma
comprobacion del CRC y el mismo desempaquetado de I/Q. Lo que pasa aqui es
lo que veria el navegador.

La placa tiene que estar flasheada con la variante del SDR
(firmware/sdkconfig.sdr), que es la unica que reserva el banco de captura:

    make build-sdr
    idf.py -B build-sdr flash

Y hay que entregarle el puerto antes, porque la consola y el navegador no
pueden escuchar a la vez:

    minino> sdr_host

uso:  python3 tools/test_esdr_web.py [puerto] [--quick]

Necesita pyserial.
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
        print(f"  \033[32mPASA\033[0m  {name}" + (f"   {detail}" if detail else ""))
    else:
        FAIL += 1
        print(f"  \033[31mFALLA\033[0m {name}" + (f"   {detail}" if detail else ""))


class Client:
    """El cliente del navegador: WebSerial a 115200 sobre el USB."""

    def __init__(self, port):
        self.ser = serial.Serial(port, 115200, timeout=5)
        self.ser.dtr = False
        self.ser.rts = False
        self.queue = b""

    def write(self, data):
        self.ser.write(data)
        self.ser.flush()

    def read(self, n, deadline=None):
        # A trozos: una lectura de 32 KB de un golpe se queda esperando a que
        # llegue todo y el pyserial avisa solo al expirar su propio timeout.
        deadline = deadline or (time.perf_counter() + 30)
        out = b""
        while len(out) < n:
            if time.perf_counter() > deadline:
                raise TimeoutError(f"se pidieron {n} bytes y llegaron {len(out)}")
            want = min(1024, n - len(out))
            chunk = self.ser.read(want)
            if chunk:
                out += chunk
        return out

    def command(self, s):
        self.write((s + "\n").encode())

    def line(self, prefix="", limit=256):
        """Igual que radio.js: lee hasta el LF y quita el CR."""
        s = ""
        end = time.perf_counter() + 5
        for _ in range(limit):
            b = self.read(1, end)[0]
            if b == 10:
                s = s.strip()
                if not s.startswith(prefix):
                    raise ValueError(f"respuesta inesperada: {s!r} (se esperaba {prefix!r})")
                return s
            s += chr(b)
        raise ValueError("respuesta demasiado larga")

    def synchronize(self):
        """Igual que radio.js: manda un LF y un marcador, y espera el eco."""
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
        raise TimeoutError("no se vio el marcador de sincronizacion")

    def capture(self, n, rate, bits):
        """Igual que radio.js: capturePacket()."""
        divider = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
                   16000000, 32000000, 10666667, 6400000].index(rate)
        self.command(f"CAP{bits * 2} {n} {divider}")
        header = self.line()
        if header.startswith("ERR "):
            raise ValueError(header)
        h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
        if not h or int(h.group(1)) != n:
            raise ValueError("cabecera de captura invalida: " + header)
        want = -(-(n * bits * 2) // 8)
        data = self.read(want, time.perf_counter() + 5)
        if zlib.crc32(data) != int(h.group(2), 16):
            raise ValueError("el CRC no coincide")
        return data, int(h.group(3))


def decode_iq(data, n, bits):
    """El decodeIQ() de radio.js, palabra por palabra."""
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
    """Un Welch basto, como el de radio.js, para ver si hay energia."""
    import math
    win = [0.5 - 0.5 * math.cos(2 * math.pi * k / (n - 1)) for k in range(n)]
    norm = sum(w * w for w in win) ** 2
    bins = [0.0] * n
    for s in range(max(1, len(iq) // 2 // n)):
        re = [iq[2 * (s * n + k)] * win[k] for k in range(n)]
        im = [iq[2 * (s * n + k) + 1] * win[k] for k in range(n)]
        # FFT de radix-2 en Python
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


print(f"\n=== ESP-WebSDR contra la placa ({PORT}) ===\n")

c = Client(PORT)

# Abrir el puerto reinicia el chip: el periférico USB-JTAG resetea cuando
# DTR cae con RTS alto (es lo que hace resetUsbJtag() en radio.js). Se
# espera a que termine el arranque antes de hablar.
print("[0] arranque (abrir el puerto reinicia el chip)")
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
check("la consola aparece tras el reinicio", b"minino>" in boot,
      boot.decode("utf-8", "replace").strip().splitlines()[-1][:50] if boot else "nada")

# ------------------------------------------------------------- entregar el puerto
print("[0] entrega del puerto (el comando sdr_host de la consola)")
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
check("la consola responde al comando sdr_host", "sdr_host" in banner or "Command" in banner,
      banner.strip().splitlines()[-1][:60] if banner.strip() else "sin salida")
check("el modo host avisa de como volver", "RELEASE" in banner and "MININO" in banner,
      banner.strip().splitlines()[-1][:60] if banner.strip() else "")
if "Command not found" in banner:
    check("el comando sdr_host existe", False, "la consola no lo conoce")
    sys.exit(1)

# ---------------------------------------------------------------- sincronizar
print("\n[1] sincronizacion (radio.js: synchronize())")
try:
    marker = c.synchronize()
    check("el marcador vuelve intacto", marker.startswith("SYNC "), marker)
except Exception as e:
    check("sincronizacion", False, str(e))
    sys.exit(1)

# ------------------------------------------------------------------- negociar
print("\n[2] negociacion (radio.js: negotiate())")
c.command("INFO")
identity = c.line()
m = re.match(r"^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$", identity)
check("INFO con la identidad que espera el visor", bool(m), identity)
if m:
    n = int(m.group(2))
    check("la longitud del burst esta entre 4096 y 16384", 4096 <= n <= 16384, str(n))
    check("es la familia C6", m.group(1) == "C6", m.group(1))
    MAX_SAMPLES = n
else:
    sys.exit(1)

c.command("CAPS")
caps = c.line()
flags = caps.split(" ")
check("CAPS no vacio", len(flags) > 1, caps)
for want in ("VERSION", "RXLIMITS", "HWAGC", "IQ8", "SERIALLEASE", "TUNEEXT", "RX40", "RX16",
              "UARTBAUD", "SPEC", "SPECN", "SPECCAPS", "DCT"):
    check(f"CAPS anuncia {want}", want in flags)
for absent in ("GPIO", "GAIN", "ALPF", "SPECSTAT"):
    check(f"CAPS NO anuncia {absent} (el visor esconderia ese control)", absent not in flags)

if "UARTBAUD" in flags:
    c.command("TRANSPORT?")
    t = re.match(r"^TRANSPORT (UART|USB) (\d+)$", c.line())
    check("TRANSPORT con el formato que espera el visor", bool(t))
    if t:
        check("es USB y el numero es 0", t.group(1) == "USB" and t.group(2) == "0",
              f"{t.group(1)} {t.group(2)}")
else:
    c.command("TRANSPORT?")
    t = re.match(r"^TRANSPORT (UART|USB) (\d+)$", c.line())
    check("TRANSPORT con el formato que espera el visor", bool(t))
    if t:
        check("es USB y el numero es 0", t.group(1) == "USB" and t.group(2) == "0",
              f"{t.group(1)} {t.group(2)}")

if "TUNEEXT" in flags:
    c.command("RANGE?")
    h = c.line("RANGE ").split(" ")
    lo, hi = int(h[1]), int(h[2])
    valid = (len(h) == 4 and lo >= 100 and lo <= 6000 and lo < hi and h[3] == "1")
    check("RANGE con el formato que espera el visor", valid, " ".join(h))
    check("el rango cubre los canales de Wi-Fi", lo <= 2412 and hi >= 2472, f"{lo}..{hi}")
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
    check("LIMITS con el JSON que espera el visor", ok, limits_line)
    g = limits.get("gain")
    check("gain es [min, max, step]", isinstance(g, list) and len(g) == 3 and
          g[0] >= 0 and g[1] >= g[0] and g[1] <= 127 and g[2] > 0 and
          (g[1] - g[0]) % g[2] == 0, str(g))
    b = limits.get("bandwidth")
    check("bandwidth es null o una cuadricula valida",
          b is None or (isinstance(b, list) and len(b) == 4), str(b))
    rates = limits.get("rates")
    known = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
             16000000, 32000000, 10666667, 6400000]
    check("rates son tasas que el visor conoce",
          isinstance(rates, list) and rates and all(r in known for r in rates) and
          len(set(rates)) == len(rates), str(rates))
    bits = limits.get("bits")
    check("bits son 8 y/o 10", isinstance(bits, list) and bits and
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
    check("VERSION con el prefijo que espera el visor", ok, line[:60])
    if ok:
        try:
            v = json.loads(line[8:])
            check("VERSION con el perfil del chip", v.get("profile") == "esp32c6",
                  str(v.get("profile")))
            check("VERSION con la revision en el formato que espera el visor",
                  isinstance(v.get("revision"), str) and
                  re.match(r"^(?:[a-f0-9]{7,40}(?:-dirty)?|unknown)$", v["revision"]) is not None,
                  str(v.get("revision")))
            check("VERSION con la marca de tiempo de 20 caracteres",
                  len(v.get("build_timestamp", "")) == 20, str(v.get("build_timestamp")))
            check("VERSION con la fecha igual a la parte de la marca",
                  v.get("build_date") == v.get("build_timestamp", "")[:10])
        except Exception as e:
            check("VERSION con un JSON valido", False, str(e))
else:
    c.command("VERSION?")

# ------------------------------------------------------------------ sintonizar
print("\n[3] sintonizacion")
c.command("FREQ 2412")
check("FREQ 2412 responde OK", c.line() == "OK")
c.command("FREQ 2417")
check("FREQ 2417 responde OK (no es un canal de Wi-Fi)", c.line() == "OK")
c.command("FREQ 2000")
r = c.line()
check("FREQ fuera de rango responde ERR", r.startswith("ERR "), r)
c.command("FREQ 2412")
c.line()

# ------------------------------------------------------------------- capturas
print("\n[4] capturas")
n = 4096 if QUICK else 16380
for bits in (8, 10):
    try:
        t0 = time.perf_counter()
        data, us = c.capture(n, 80000000, bits)
        dt = time.perf_counter() - t0
        kbps = len(data) / dt / 1000.0
        check(f"CAP{bits*2} de {n} muestras", True,
              f"{len(data)} bytes, {us} us de captura, {dt*1000:.0f} ms en total, "
              f"{kbps:.0f} kB/s")
        iq = decode_iq(data, n, bits)
        peak = max(abs(x) for x in iq)
        rms = (sum(x * x for x in iq) / len(iq)) ** 0.5
        check(f"CAP{bits*2}: los valores estan en el rango",
              all(-1.01 <= x <= 1.01 for x in iq), f"pico {peak:.3f}")
        check(f"CAP{bits*2}: hay senal (no es ruido plano)", rms > 0.001,
              f"rms {rms:.5f}, pico {peak:.3f}")
    except Exception as e:
        check(f"CAP{bits*2} de {n} muestras", False, str(e))

# --------------------------------------------------------------------- tasas
print("\n[5] tasas")
for rate in RX_RATES:
    code = [80000000, 40000000, 20000000, 10000000, 8000000, 4000000,
            16000000, 32000000, 10666667, 6400000].index(rate)
    c.command(f"CAP16 4096 {code}")
    header = c.line()
    if header.startswith("ERR "):
        check(f"CAP16 a {rate/1e6:g} MS/s", False, header)
        continue
    h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
    if not h:
        check(f"CAP16 a {rate/1e6:g} MS/s", False, header)
        continue
    data = c.read(4096 * 2, time.perf_counter() + 5)
    ok = zlib.crc32(data) == int(h.group(2), 16)
    check(f"CAP16 a {rate/1e6:g} MS/s", ok, f"{int(h.group(3))} us de captura")

# ------------------------------------------------------------------- spectrum
if not QUICK:
    print("\n[6] espectro de la captura")
    try:
        data, us = c.capture(16380, 80000000, 10)
        iq = decode_iq(data, 16380, 10)
        spec = spectrum(iq, 4096)
        peak = max(spec)
        floor = min(spec)
        print(f"     pico {peak:.1f} dBFS, suelo {floor:.1f} dBFS, "
              f"span {peak-floor:.1f} dB")
        check("el espectro tiene dynamic range", peak - floor > 3.0,
              f"{peak-floor:.1f} dB")
        # la banda de 2.4 GHz esta en el centro del bin 0
        check("el pico esta en el centro (LO en 2412 MHz)",
              spec[0] >= max(spec[1:1000]) - 3.0, f"centro {spec[0]:.1f} dB")
    except Exception as e:
        check("espectro", False, str(e))

# ------------------------------------------------------------------ release
print("\n[7] release y escape")
c.command("RELEASE")
check("RELEASE responde OK", c.line() == "OK")

# Tras RELEASE la sesion se acaba y la consola vuelve: el prompt reaparece.
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
check("tras RELEASE vuelve la consola", b"minino>" in back,
      back.decode("utf-8", "replace").strip().splitlines()[-1][:50] if back else "nada")

# El escape propio de Minino: la consola vuelve sin que el navegador mande nada.
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
check("MININO devuelve la consola", b"minino>" in esc,
      esc.decode("utf-8", "replace").strip().splitlines()[-1][:50] if esc else "nada")

print(f"\n=== {OK} pasan, {FAIL} fallan ===\n")
sys.exit(1 if FAIL else 0)
