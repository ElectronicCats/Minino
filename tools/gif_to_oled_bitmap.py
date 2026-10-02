#!/usr/bin/env python3
"""Convert a GIF animation to Minino OLED 1-bit C bitmap headers.

Minino OLED layout (oled_driver_bitmaps):
  - width must be a multiple of 8
  - row-major order
  - each byte = 8 horizontal pixels, MSB = leftmost
  - 1 bit per pixel (1 = pixel on)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required: pip install Pillow")


def gif_to_bitmap_frames(gif_path, width, height, max_frames, threshold=128,
                         invert=False):
    """Return (frames_bytes, durations_ms, skipped_count).

    Each frame is packed to bytes: row-major, 8 pixels/byte, MSB left.
    Only distinct frames are kept (dedupe identical consecutive frames).
    invert: dark pixels become lit (white-on-black OLED style).
    """
    im = Image.open(gif_path)
    n_frames = getattr(im, "n_frames", 1)

    frames = []
    durations = []
    seen = set()
    skipped = 0

    for i in range(n_frames):
        im.seek(i)
        frame = im.convert("L")  # grayscale
        if frame.size != (width, height):
            frame = frame.resize((width, height), Image.LANCZOS)

        # Threshold to 1-bit. By default bright pixels are lit.
        if invert:
            mono = frame.point(lambda p: 255 if p < threshold else 0, mode="1")
        else:
            mono = frame.point(lambda p: 255 if p > threshold else 0, mode="1")

        # Pack pixels row-major, 8 per byte, MSB left
        px = mono.load()
        data = bytearray()
        for y in range(height):
            for xb in range(width // 8):
                byte = 0
                for bit in range(8):
                    if px[xb * 8 + bit, y]:
                        byte |= 0x80 >> bit
                data.append(byte)

        key = bytes(data)
        if key in seen:
            skipped += 1
            continue
        seen.add(key)

        frames.append(data)
        durations.append(im.info.get("duration", 100))

        if len(frames) >= max_frames:
            break

    return frames, durations, skipped


def format_array(name, data, per_line=12):
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), per_line):
        chunk = ", ".join(f"0x{b:02x}" for b in data[i : i + per_line])
        lines.append(f"    {chunk},")
    lines.append("};")
    return "\n".join(lines)


def generate_header(name, frames, durations, width, height):
    guard = f"{name.upper()}_H"
    out = []
    out.append(f"#ifndef {guard}")
    out.append(f"#define {guard}")
    out.append("")
    out.append(f"#include <stddef.h>")
    out.append(f"#include <stdint.h>")
    out.append("")
    out.append(f"#define {name.upper()}_WIDTH  {width}")
    out.append(f"#define {name.upper()}_HEIGHT {height}")
    out.append(f"#define {name.upper()}_FRAMES {len(frames)}")
    out.append("")

    for i, data in enumerate(frames):
        out.append(format_array(f"{name}_frame_{i}", data))
        out.append("")

    out.append(f"static const unsigned char* const {name}_frames[] = {{")
    for i in range(len(frames)):
        out.append(f"    {name}_frame_{i},")
    out.append("};")
    out.append("")

    out.append(f"static const uint32_t {name}_durations_ms[] = {{")
    out.append("    " + ", ".join(str(d) for d in durations))
    out.append("};")
    out.append("")
    out.append(f"#endif  // {guard}")
    out.append("")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description="GIF -> Minino OLED 1-bit C bitmap header")
    ap.add_argument("input", help="Input GIF path")
    ap.add_argument("-o", "--output", required=True, help="Output .h path")
    ap.add_argument("-n", "--name", default="rf_village", help="Base symbol name")
    ap.add_argument("--width", type=int, default=128)
    ap.add_argument("--height", type=int, default=64)
    ap.add_argument("--max-frames", type=int, default=12)
    ap.add_argument("--threshold", type=int, default=128)
    ap.add_argument("--invert", action="store_true",
                    help="Dark pixels become lit (white-on-black OLED style)")
    args = ap.parse_args()

    if args.width % 8 != 0:
        sys.exit("width must be a multiple of 8")

    frames, durations, skipped = gif_to_bitmap_frames(
        args.input, args.width, args.height, args.max_frames, args.threshold,
        args.invert,
    )

    header = generate_header(args.name, frames, durations, args.width, args.height)
    Path(args.output).write_text(header)

    total_bytes = sum(len(f) for f in frames)
    print(f"Wrote {args.output}")
    print(f"  Frames kept: {len(frames)} (skipped {skipped} duplicates)")
    print(f"  Frame size:  {len(frames[0])} bytes ({args.width}x{args.height})")
    print(f"  Total:       {total_bytes} bytes")
    print(f"  Durations:   {durations}")


if __name__ == "__main__":
    main()
