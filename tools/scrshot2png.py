#!/usr/bin/env python3
"""scrshot2png.py - convert a PSPLink `scrshot` raw capture to PNG.

Layout (measured, P10-S13):
  * a 54-byte BMP header, then
  * 480 x 272 pixels, 3 bytes each, **B,G,R**, **bottom-up** (raw row 0 is the last
    display line), already quantised to the GE's 5-6-5 RGB.

Reading the body as top-down RGB swaps red/blue and flips the image; both happened
before this tool existed. The header length is derived as `len - 480*272*3`, so a
capture of another size fails loudly instead of being shifted.

Usage:
  tools/scrshot2png.py shot.raw shot.png
  tools/scrshot2png.py shot.raw --probe 240,136
  tools/scrshot2png.py shot.raw shot.png --probe 10,10
"""

import argparse
import struct
import sys
import zlib

WIDTH = 480
HEIGHT = 272
HEADER_BYTES = 54


def fail(message):
    print("scrshot2png: " + message, file=sys.stderr)
    raise SystemExit(2)


def read_raw(path):
    try:
        data = open(path, "rb").read()
    except OSError as exc:
        fail(str(exc))

    header = len(data) - WIDTH * HEIGHT * 3
    if header != HEADER_BYTES:
        fail("expected a %d-byte header for %dx%d, got %d bytes (file %d)"
             % (HEADER_BYTES, WIDTH, HEIGHT, header, len(data)))
    if data[:2] != b"BM":
        fail("header does not start with BM")

    body = data[header:]
    rows = []
    for y in range(HEIGHT):
        start = (HEIGHT - 1 - y) * WIDTH * 3        # raw is bottom-up
        src = body[start:start + WIDTH * 3]
        row = bytearray(WIDTH * 3)
        row[0::3] = src[2::3]                       # R <- B
        row[1::3] = src[1::3]                       # G
        row[2::3] = src[0::3]                       # B <- R
        rows.append(bytes(row))
    return rows


def write_png(rows, path):
    signature = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0)
    scanlines = b"".join(b"\x00" + row for row in rows)   # filter: none
    idat = zlib.compress(scanlines, 9)

    out = bytearray(signature)
    for kind, payload in ((b"IHDR", ihdr), (b"IDAT", idat), (b"IEND", b"")):
        out += struct.pack(">I", len(payload)) + kind + payload
        out += struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    try:
        open(path, "wb").write(out)
    except OSError as exc:
        fail(str(exc))


def probe(rows, x, y):
    if (x < 0) or (y < 0) or (x >= WIDTH) or (y >= HEIGHT):
        fail("probe %d,%d is outside %dx%d" % (x, y, WIDTH, HEIGHT))
    r, g, b = rows[y][x * 3:x * 3 + 3]
    print("%d,%d: #%02x%02x%02x (%d,%d,%d)" % (x, y, r, g, b, r, g, b))


def main():
    parser = argparse.ArgumentParser(description="Convert a PSPLink scrshot raw to PNG.")
    parser.add_argument("raw", help="input .raw from pspsh scrshot")
    parser.add_argument("png", nargs="?", help="output PNG path")
    parser.add_argument("--probe", metavar="X,Y", help="print the RGB at screen x,y")
    args = parser.parse_args()

    if (args.png is None) and (args.probe is None):
        parser.error("give an output PNG and/or --probe X,Y")

    rows = read_raw(args.raw)
    if args.png is not None:
        write_png(rows, args.png)
        print("wrote %s" % args.png)
    if args.probe is not None:
        try:
            x, y = (int(v) for v in args.probe.split(",", 1))
        except ValueError:
            fail("--probe expects X,Y")
        probe(rows, x, y)


if __name__ == "__main__":
    main()
