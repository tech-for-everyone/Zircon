#!/usr/bin/env python3
"""Convert an 8-bit non-interlaced RGBA PNG into a scaled RGB565 boot image."""

from __future__ import annotations

import argparse
import struct
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
OUTPUT_SIZE = 192
BACKGROUND = (0, 0, 0)


def decode_rgba_png(path: str) -> tuple[int, int, bytes]:
    with open(path, "rb") as source:
        data = source.read()
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("input is not a PNG")

    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    offset = len(PNG_SIGNATURE)
    while offset < len(data):
        if offset + 12 > len(data):
            raise ValueError("truncated PNG chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        start = offset + 8
        end = start + length
        if end + 4 > len(data):
            raise ValueError("truncated PNG data")
        chunk = data[start:end]
        if kind == b"IHDR":
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk)
            )
            if compression != 0 or filtering != 0:
                raise ValueError("unsupported PNG compression or filter method")
        elif kind == b"IDAT":
            compressed.extend(chunk)
        elif kind == b"IEND":
            break
        offset = end + 4

    if width is None or height is None or width <= 0 or height <= 0:
        raise ValueError("missing or invalid PNG dimensions")
    if bit_depth != 8 or color_type != 6 or interlace != 0:
        raise ValueError("expected an 8-bit, non-interlaced RGBA PNG")

    raw = zlib.decompress(compressed)
    row_bytes = width * 4
    if len(raw) != height * (row_bytes + 1):
        raise ValueError("PNG decompressed size does not match its dimensions")

    pixels = bytearray(height * row_bytes)
    src = 0
    for y in range(height):
        filter_type = raw[src]
        src += 1
        row_start = y * row_bytes
        previous_start = row_start - row_bytes
        for x in range(row_bytes):
            value = raw[src]
            src += 1
            left = pixels[row_start + x - 4] if x >= 4 else 0
            above = pixels[previous_start + x] if y else 0
            upper_left = pixels[previous_start + x - 4] if y and x >= 4 else 0
            if filter_type == 1:
                value += left
            elif filter_type == 2:
                value += above
            elif filter_type == 3:
                value += (left + above) // 2
            elif filter_type == 4:
                p = left + above - upper_left
                pa, pb, pc = abs(p - left), abs(p - above), abs(p - upper_left)
                value += left if pa <= pb and pa <= pc else above if pb <= pc else upper_left
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter type {filter_type}")
            pixels[row_start + x] = value & 0xFF
    return width, height, bytes(pixels)


def make_boot_image(width: int, height: int, rgba: bytes) -> bytes:
    output = bytearray()
    for y in range(OUTPUT_SIZE):
        sy = min(height - 1, (y * height) // OUTPUT_SIZE)
        for x in range(OUTPUT_SIZE):
            sx = min(width - 1, (x * width) // OUTPUT_SIZE)
            i = (sy * width + sx) * 4
            alpha = rgba[i + 3]
            red = (rgba[i] * alpha + BACKGROUND[0] * (255 - alpha)) // 255
            green = (rgba[i + 1] * alpha + BACKGROUND[1] * (255 - alpha)) // 255
            blue = (rgba[i + 2] * alpha + BACKGROUND[2] * (255 - alpha)) // 255
            rgb565 = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
            output.extend(struct.pack("<H", rgb565))
    return bytes(output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="source RGBA PNG")
    parser.add_argument("output", help="destination raw RGB565 image")
    args = parser.parse_args()
    width, height, rgba = decode_rgba_png(args.input)
    image = make_boot_image(width, height, rgba)
    with open(args.output, "wb") as target:
        target.write(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
