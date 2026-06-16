#!/usr/bin/env python3
"""
NovaOS LNP tool.

LNP = Nova Picture, a tiny image format for NovaOS tools.
This is an external development tool: it runs on Linux/Windows, not inside NovaOS.

Format v1:
  bytes 0..3   magic: LNP1
  bytes 4..5   width, little-endian unsigned 16-bit
  bytes 6..7   height, little-endian unsigned 16-bit
  bytes 8..    pixels in RGB order: R, G, B, R, G, B...
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAGIC = b"LNP1"
HEADER = struct.Struct("<4sHH")


def write_lnp(path: Path, width: int, height: int, pixels: bytes) -> None:
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive")
    if width > 65535 or height > 65535:
        raise ValueError("LNP v1 stores width/height as 16-bit values")
    expected = width * height * 3
    if len(pixels) != expected:
        raise ValueError(f"expected {expected} RGB bytes, got {len(pixels)}")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(HEADER.pack(MAGIC, width, height) + pixels)


def read_lnp(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if len(data) < HEADER.size:
        raise ValueError("file too small")

    magic, width, height = HEADER.unpack(data[:HEADER.size])
    if magic != MAGIC:
        raise ValueError("not an LNP1 file")

    pixels = data[HEADER.size:]
    expected = width * height * 3
    if len(pixels) != expected:
        raise ValueError(f"bad pixel data: expected {expected} bytes, got {len(pixels)}")
    return width, height, pixels


def _read_ppm_token(data: bytes, pos: int) -> tuple[bytes, int]:
    while pos < len(data):
        c = data[pos]
        if c in b" \t\r\n":
            pos += 1
            continue
        if c == ord("#"):
            while pos < len(data) and data[pos] not in b"\r\n":
                pos += 1
            continue
        break

    start = pos
    while pos < len(data) and data[pos] not in b" \t\r\n#":
        pos += 1
    return data[start:pos], pos


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    pos = 0
    magic, pos = _read_ppm_token(data, pos)
    if magic not in (b"P3", b"P6"):
        raise ValueError("only P3/P6 PPM images are supported without extra libraries")

    w_token, pos = _read_ppm_token(data, pos)
    h_token, pos = _read_ppm_token(data, pos)
    max_token, pos = _read_ppm_token(data, pos)
    width = int(w_token)
    height = int(h_token)
    max_value = int(max_token)
    if max_value <= 0 or max_value > 255:
        raise ValueError("only PPM max value 1..255 is supported")

    if magic == b"P6":
        while pos < len(data) and data[pos] in b" \t\r\n":
            pos += 1
        pixels = data[pos:pos + width * height * 3]
        if max_value != 255:
            pixels = bytes((v * 255) // max_value for v in pixels)
        return width, height, pixels

    values: list[int] = []
    for _ in range(width * height * 3):
        token, pos = _read_ppm_token(data, pos)
        if not token:
            raise ValueError("PPM ended before all pixels were read")
        values.append((int(token) * 255) // max_value)
    return width, height, bytes(values)


def write_ppm(path: Path, width: int, height: int, pixels: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = f"P6\n{width} {height}\n255\n".encode("ascii")
    path.write_bytes(header + pixels)


def make_sample(path: Path, width: int, height: int) -> None:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            star = (x * 17 + y * 31) % 97 == 0
            nebula = (x - width // 2) * (x - width // 2) + (y - height // 2) * (y - height // 2)
            blue = 30 + (x * 90) // max(width, 1)
            purple = 20 + (y * 70) // max(height, 1)
            if star:
                pixels.extend((240, 240, 210))
            elif nebula < (min(width, height) * min(width, height)) // 10:
                pixels.extend((90, 40 + purple, 160))
            else:
                pixels.extend((5, 8 + purple // 4, blue))
    write_lnp(path, width, height, bytes(pixels))


def cmd_info(args: argparse.Namespace) -> None:
    width, height, pixels = read_lnp(Path(args.input))
    print(f"LNP1 image: {width}x{height}, {len(pixels)} RGB bytes")


def cmd_from_ppm(args: argparse.Namespace) -> None:
    width, height, pixels = read_ppm(Path(args.input))
    write_lnp(Path(args.output), width, height, pixels)
    print(f"Wrote {args.output}: {width}x{height}")


def cmd_to_ppm(args: argparse.Namespace) -> None:
    width, height, pixels = read_lnp(Path(args.input))
    write_ppm(Path(args.output), width, height, pixels)
    print(f"Wrote {args.output}: {width}x{height}")


def cmd_sample(args: argparse.Namespace) -> None:
    make_sample(Path(args.output), args.width, args.height)
    print(f"Wrote sample {args.output}: {args.width}x{args.height}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="NovaOS .lnp image tool")
    sub = parser.add_subparsers(required=True)

    info = sub.add_parser("info", help="show information about an .lnp file")
    info.add_argument("input")
    info.set_defaults(func=cmd_info)

    from_ppm = sub.add_parser("from-ppm", help="convert PPM P3/P6 to .lnp")
    from_ppm.add_argument("input")
    from_ppm.add_argument("output")
    from_ppm.set_defaults(func=cmd_from_ppm)

    to_ppm = sub.add_parser("to-ppm", help="convert .lnp to PPM P6")
    to_ppm.add_argument("input")
    to_ppm.add_argument("output")
    to_ppm.set_defaults(func=cmd_to_ppm)

    sample = sub.add_parser("sample", help="create a small galaxy .lnp sample")
    sample.add_argument("output")
    sample.add_argument("--width", type=int, default=64)
    sample.add_argument("--height", type=int, default=36)
    sample.set_defaults(func=cmd_sample)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
