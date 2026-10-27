#!/usr/bin/env python3
"""Drive the ESP-SDR receiver over USB and check it is producing I/Q samples.

The radio has no display on purpose: its output is a stream of I/Q samples
that the host renders. Minino's whole role is choosing which partition boots;
everything visible happens here.

This is a smoke test. It tunes the receiver, pulls a burst, reports the signal
statistics and writes the raw bytes so they can be opened in the ESPARGOS
viewer (https://espargos.net/espsdr/app/).

    python3 firmware/tools/sdr_capture.py --port /dev/ttyACM0 --freq 2400

Baud rate is irrelevant over USB Serial/JTAG, but the port has to be opened
before Minino releases it, so run this after the SDR has booted.
"""
import argparse
import math
import sys
import time

try:
    import serial
except ImportError:
    sys.exit('This needs pyserial: pip install pyserial')


class Sdr:
    def __init__(self, port, timeout=1.0):
        self.ser = serial.Serial(port, 115200, timeout=timeout)
        self.ser.dtr = False
        self.ser.rts = False
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def read_line(self, deadline):
        line = b''
        while time.time() < deadline:
            byte = self.ser.read(1)
            if byte == b'\n':
                if line:
                    return line.decode('ascii', 'replace').strip()
                continue
            if byte:
                line += byte
        return None

    def drain(self, quiet=0.25):
        """Discards anything already in flight.

        The receiver batches its replies, so without this a leftover line from
        the previous command gets read back as if it were this one's answer.
        """
        deadline = time.time() + quiet
        while time.time() < deadline:
            if not self.ser.read(512):
                break
            deadline = time.time() + quiet

    def ask(self, command, timeout=5.0):
        self.drain()
        self.ser.write((command + '\n').encode())
        time.sleep(0.2)
        return self.read_line(time.time() + timeout)

    def sync(self, attempts=10):
        """Waits until the receiver answers.

        Opening the port pulses DTR/RTS, which resets the board. That is
        harmless here, but it means the first commands are sent into a reboot,
        so keep asking until something answers.
        """
        for attempt in range(attempts):
            self.ser.reset_input_buffer()
            if attempt:
                time.sleep(1.0)
            reply = self.ask('TRANSPORT?', timeout=2.0)
            if reply and reply.startswith('TRANSPORT'):
                return reply
        sys.exit('The receiver never answered TRANSPORT?. Is the SDR booted?')

    def read_exact(self, count, deadline):
        out = bytearray()
        while len(out) < count and time.time() < deadline:
            chunk = self.ser.read(count - len(out))
            if chunk:
                out += chunk
        return bytes(out)


IQ_BITS = 10
IQ_CENTRE = 1 << (IQ_BITS - 1)


def unpack_iq_pairs(payload, pairs):
    """Unpacks the packed format the burst protocol uses.

    CAP20 carries 20 bits per pair: 10 for I followed by 10 for Q, little
    endian, with pairs packed back to back and no padding between them. CAP16
    is the same with 8 bit components.
    """
    stride = IQ_BITS * 2
    total = len(payload) * 8
    out = []
    bit = 0
    for _ in range(pairs):
        if bit + stride > total:
            break
        # The last pair can need up to three bytes past the end, so pad rather
        # than drop it; the extra bits are outside the 20 we keep.
        start = bit // 8
        window = payload[start:start + 4].ljust(4, b'\x00')
        value = int.from_bytes(window, 'little') >> (bit % 8)
        out.append((((value & 0x3FF) - IQ_CENTRE),
                    (((value >> IQ_BITS) & 0x3FF) - IQ_CENTRE)))
        bit += stride
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse
                                     .RawDescriptionHelpFormatter)
    parser.add_argument('--port', default='/dev/ttyACM0')
    parser.add_argument('--freq', type=int, default=2400,
                        help='MHz, 100 to 6000')
    parser.add_argument('--count', type=int, default=4096,
                        help='samples to capture')
    parser.add_argument('--rate', type=int, default=0,
                        help='decimation rate; 0 is full speed')
    parser.add_argument('--out', default='sdr_capture.bin',
                        help='where to write the raw burst')
    args = parser.parse_args()

    sdr = Sdr(args.port)
    try:
        print('transport :', sdr.sync())
        version = sdr.ask('VERSION?')
        print('version   :', (version or '')[:96])
        limits = sdr.ask('LIMITS?')
        print('limits    :', (limits or '')[:96])

        tune = sdr.ask('FREQ %d' % args.freq)
        if tune != 'OK':
            sys.exit('Tuning to %d MHz was refused: %r' % (args.freq, tune))
        print('tuned     : %d MHz' % args.freq)

        reply = sdr.ask('CAP20 %d %d' % (args.count, args.rate), timeout=3.0)
        if not reply or not reply.startswith('DATA '):
            sys.exit('Capture refused: %r' % reply)
        fields = reply.split()
        count = int(fields[1])
        payload = sdr.read_exact(count * IQ_BITS * 2 // 8, time.time() + 10.0)
        sdr.read_line(time.time() + 2.0)  # trailing END

        with open(args.out, 'wb') as handle:
            handle.write(payload)
        print('captured  : %d samples, %d bytes -> %s' % (count, len(payload),
                                                         args.out))

        pairs = unpack_iq_pairs(payload, count)
        if len(pairs) < count:
            sys.exit('Only decoded %d of %d pairs, burst was truncated'
                     % (len(pairs), count))
        # Compare like with like: rms of the complex magnitude against the peak
        # magnitude, not against the largest single component.
        magnitudes = [math.hypot(i, q) for i, q in pairs]
        rms = math.sqrt(sum(m * m for m in magnitudes) / len(magnitudes))
        peak = max(magnitudes)
        print('amplitude : rms %.1f, peak %.1f (%d bit, center +-%d)'
              % (rms, peak, IQ_BITS, IQ_CENTRE))
        if not 0.0 <= rms <= peak:
            sys.exit('Internal error: rms %.1f outside [0, %.1f], '
                     'the unpacking is wrong' % (rms, peak))
        print('           I range %d..%d, Q range %d..%d'
              % (min(i for i, _ in pairs), max(i for i, _ in pairs),
                 min(q for _, q in pairs), max(q for _, q in pairs)))

        # A dead receiver returns a flat line; a working one has energy.
        if rms < 1.0:
            print('WARNING   : the burst is flat, the receiver may not be running')
        else:
            print('OK         : the receiver is delivering I/Q samples')
        return 0
    finally:
        sdr.close()


if __name__ == '__main__':
    sys.exit(main())