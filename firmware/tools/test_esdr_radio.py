#!/usr/bin/env python3
"""Prueba sobre el aire: una placa transmite y la otra recibe con el SDR.

La placa de recepcion entra en el modo host (el mismo protocolo que habla
el visor del navegador) y mide; la placa de transmision alterna entre una
app que radia (el spammer de BLE o el de beacons de Wi-Fi) y la de no
hacer nada.

Dos medidas, según el modo:

  spec  La placa calcula la FFT (el modo SPEC) y se compara el espectro
        medio con la transmision puesta contra el de las ventanas sin
        transmision. Es lo que veria el visor del navegador.

  iq    Se piden rafagas de I/Q (CAP20) y se mide la potencia estrecha que
        aparece en una ventana de 1 MHz alrededor de la frecuencia
        buscada. Una senal estrecha (un anuncio de BLE son 1 MHz) se ve
        como un pico; el trafico de Wi-Fi de alrededor ocupa 11 MHz y
        reparte su energia, asi que la ventana estrecha lo distingue.

    uso:  python3 tools/test_esdr_radio.py <puerto_rx> <puerto_tx> [opciones]

Opciones:
    --mode M     "spec" (por defecto) o "iq"
    --on CMD     comando de consola que arranca la transmision (por defecto
                 "launch spam_all"). Varios comandos separados por ';' se
                 mandan en orden (el spammer de beacons necesita abrir el
                 menu y pulsar Start: "launch ssid_spam;press down;press
                 right")
    --off CMD    comando que la para (por defecto "exit_app")
    --rate HZ    tasa de muestreo del espectro (80000000, 40000000, 16000000)
    --bins N     bins de la FFT (256, 512, 1024, 2048)
    --seconds N  segundos de medida
    --period N   segundos con cada estado
    --freq HZ    frecuencia a la que se sintoniza la placa de recepcion
    --freqs HZ   frecuencias (en MHz) que se miden en el modo iq, separadas
                 por espacios (por defecto, los canales de publicidad de
                 BLE y algunos de referencia)

Necesita pyserial.
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
    """La consola de Minino: las lineas se cierran con CR, no con LF."""

    def __init__(self, port):
        self.port = port
        self.raw = serial.Serial(port, 115200, timeout=5)
        self.raw.dtr = False
        self.raw.rts = False

    def send(self, text):
        """Manda una linea. Varias separadas por ';' se mandan en orden."""
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
    """Mete y saca la transmision en la otra placa.

    Cada cambio de estado abre una ventana nueva: las ventanas se
    numeran, y asi cada ventana de transmision se puede comparar con las
    ventanas sin transmision que la rodean (el entorno de RF cambia solo
    con el tiempo, y comparar con las ventanas vecinas lo cancela).
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
    raise TimeoutError("no llego la respuesta")


def open_host_session(port):
    """Entrega el puerto con el comando sdr_host de la consola."""
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
        print("la consola no aparecio en el puerto de recepcion")
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
        print("el modo host no arranco:", banner[-200:])
        sys.exit(1)
    print("modo host listo")
    return ser


def decode_iq20(data, n):
    """CAP20: 20 bits por par, I en los bits 0-9 y Q en los 10-19."""
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
    """FFT de radix-2 in situ: la entrada es la parte real y la imaginaria."""
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
    """Una rafaga de I/Q en formato de 20 bits por par."""
    header = ""
    for intento in range(4):
        ser.write(f"CAP20 {samples} 0\n".encode())
        header = read_line(ser, 15.0)
        if header.startswith("DATA "):
            break
        # el puerto puede dejar basura: se busca la cabecera de verdad
        print("    (linea inesperada:", header[:40], ")")
    if header.startswith("ERR "):
        raise ValueError(header)
    h = re.match(r"^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$", header)
    if not h:
        raise ValueError("cabecera invalida: " + header)
    want = -(-(samples * 20) // 8)
    data = b""
    end = time.perf_counter() + 10
    while len(data) < want and time.perf_counter() < end:
        chunk = ser.read(min(4096, want - len(data)))
        if chunk:
            data += chunk
    if zlib.crc32(data) != int(h.group(2), 16):
        # una rafaga puede llegar cortada: se repite
        if intento + 1 < 4:
            return capture_iq(ser, samples)
        raise ValueError("el CRC no coincide")
    return decode_iq20(data, samples)


def mode_iq(ser, toggle, freqs_hz, lo_hz, samples, rate, seconds):
    """La potencia estrecha en varias frecuencias, con y sin transmision.

    En cada rafaga se mide la potencia de una ventana estrecha alrededor de
    cada frecuencia pedida. Una señal estrecha (un anuncio de BLE son 1 MHz)
    solo aparece en su canal; el trafico del entorno, que es de 11 MHz de
    ancho, reparte su energia y queda repartido por todas las ventanas.
    """
    on = []
    off = []
    # la FFT de radix-2 quiere una potencia de dos: la rafaga se rellena
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
            # el desplazamiento puede ser negativo: la FFT va de -rate/2 a
            # +rate/2, asi que el bin es el desplazamiento con signo
            k = int((freq - lo_hz) / bin_hz) % n
            p = 0.0
            for j in range(max(0, k - half), min(n, k + half + 1)):
                p += re[j] ** 2 + im[j] ** 2
            return 10 * math.log10(p / n + 1e-20)

        row = [window(f) for f in freqs_hz]
        (on if toggle.state_now() else off).append(row)
        time.sleep(0.05)

    print(f"\n  potencia en ventanas de {2 * half * bin_hz / 1e6:.2f} MHz "
          f"(LO {lo_hz / 1e6:g} MHz), maxima sobre las rafagas:")
    print(f"   {'frecuencia':>12} {'TX ON':>9} {'TX OFF':>9} {'dif':>8}   "
          f"{'p90 ON':>9} {'p90 OFF':>9} {'dif':>8}")
    for i, f in enumerate(freqs_hz):
        a = sorted(r[i] for r in on)
        b = sorted(r[i] for r in off)
        if not a or not b:
            continue
        p90a = a[int(len(a) * 0.9)]
        p90b = b[int(len(b) * 0.9)]
        print(f"   {f / 1e6:>9.1f} MHz {a[-1]:>9.2f} {b[-1]:>9.2f} "
              f"{a[-1] - b[-1]:>+8.2f}   {p90a:>9.2f} {p90b:>9.2f} {p90a - p90b:>+8.2f}")
    print(f"\n  rafagas: TX ON {len(on)}, TX OFF {len(off)}")


def mode_spec(ser, toggle, rate, bins, seconds):
    """El espectro de la placa (modo SPEC) con y sin transmision."""
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
        print(f"no hay perfil de {bins} bins a {rate / 1e6:g} MS/s")
        sys.exit(1)
    rate_code, stride, upf = int(match[0][1]), int(match[0][3]), int(match[0][4])
    print(f"perfil: {match[0]}")

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
    print(f"\nmarcos: TX ON {len(on)} ({len([1 for k in windows if k[1]])} ventanas), "
          f"TX OFF {len(off)} ({len([1 for k in windows if not k[1]])} ventanas)")
    if not on or not off:
        print("no hay marcos suficientes para comparar")
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

    # Comparacion por parejas: cada ventana con transmision se compara con
    # las ventanas sin transmision que la rodean. El entorno de RF se mueve
    # solo con el tiempo, y emparejarlo con las ventanas vecinas lo
    # cancela; lo que queda es la senal del transmisor.
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
        print(f"  comparacion por parejas: {len(paired)} ventanas con transmision "
              f"contra sus vecinas")
    else:
        diff_paired = diff

    hz = rate / bins
    print(f"  TX ON : pico {max(m_on):.0f}  mediana {sorted(m_on)[bins // 2]:.0f}  "
          f"suma {sum(m_on):.0f}")
    print(f"  TX OFF: pico {max(m_off):.0f}  mediana {sorted(m_off)[bins // 2]:.0f}  "
          f"suma {sum(m_off):.0f}")

    print("\n  las diez bins con mas diferencia (TX ON - TX OFF, por parejas):")
    for t in sorted(range(bins), key=lambda k: -diff_paired[k])[:10]:
        print(f"   {t * hz / 1e6:+8.3f} MHz del LO   dif {diff_paired[t]:+6.1f}")

    span = int(rate / 2 / 1e6)
    print("\n  diferencia por banda de 1 MHz (por parejas):")
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

    print(f"=== Prueba sobre el aire: RX {rx_port}, TX {tx_port} ===\n")
    print(f"  modo: {mode}")
    print(f"  transmision: '{on_cmd}' / '{off_cmd}'")
    if mode == "spec":
        print(f"  espectro: {rate / 1e6:g} MS/s, {bins} bins, {seconds:g} s")
    else:
        print(f"  I/Q: {samples} muestras a {rate / 1e6:g} MS/s, "
              f"LO {freq} MHz, frecuencias {[f // 1000000 for f in freqs]} MHz, "
              f"{seconds:g} s")

    tx = Console(tx_port)
    tx.drain(0.5)

    ser = open_host_session(rx_port)
    ser.write(f"FREQ {freq}\n".encode())
    print("sintonía:", read_line(ser, 10.0))

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
