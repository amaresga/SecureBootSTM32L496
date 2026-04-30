#!/usr/bin/env python3
"""
patch_image.py — fill in the length and CRC-32 fields of the app image header.

Usage:
  python patch_image.py <bin_file> <flash_base_hex>

The script:
  1. Verifies the magic word at offset 0.
  2. Writes total image size (bytes) into the length field  (offset  8).
  3. Computes CRC-32/ISO-HDLC over bytes [16..length-1] and writes it
     into the crc32 field (offset 12).
  4. Saves the patched binary in place.

The CRC algorithm matches Python's binascii.crc32() and the C implementation
in bootloader/src/boot/boot.c (poly 0xEDB88320, reflected, init/xor 0xFFFFFFFF).
"""

import sys
import struct
import binascii

IMAGE_MAGIC = 0x5AA5A55A
HDR_FIELDS  = 16   # bytes occupied by the four header words (magic/version/length/crc32)


def main() -> None:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bin_file> <flash_base_hex>", file=sys.stderr)
        sys.exit(1)

    bin_path   = sys.argv[1]
    flash_base = int(sys.argv[2], 16)

    with open(bin_path, "rb") as fh:
        data = bytearray(fh.read())

    if len(data) < HDR_FIELDS:
        print(f"ERROR: binary is only {len(data)} bytes — too small to contain a header",
              file=sys.stderr)
        sys.exit(1)

    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != IMAGE_MAGIC:
        print(f"ERROR: magic 0x{magic:08X} != expected 0x{IMAGE_MAGIC:08X}", file=sys.stderr)
        sys.exit(1)

    length = len(data)

    # CRC covers everything after the four header words
    crc = binascii.crc32(data[HDR_FIELDS:]) & 0xFFFFFFFF

    struct.pack_into("<I", data, 8,  length)
    struct.pack_into("<I", data, 12, crc)

    with open(bin_path, "wb") as fh:
        fh.write(data)

    print(f"  image header patched:")
    print(f"    base    = 0x{flash_base:08X}")
    print(f"    length  = 0x{length:08X}  ({length} bytes)")
    print(f"    crc32   = 0x{crc:08X}")


if __name__ == "__main__":
    main()
