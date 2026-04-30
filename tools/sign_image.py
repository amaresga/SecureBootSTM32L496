#!/usr/bin/env python3
"""
sign_image.py -- Patch the application image header and sign it with Ed25519.

    python tools\\sign_image.py build\\app\\Release\\App.bin

The private key passphrase can be supplied via the SIGNING_PASSPHRASE
environment variable or entered interactively when prompted.

Patching performed (offsets relative to APP_BASE = 0x08080000):
  0x08  length      <- total binary size in bytes
  0x0C  crc32       <- CRC-32/ISO-HDLC over body [APP_BODY_OFFSET..length-1]
  0x10  public_key  <- 32-byte Ed25519 public key
  0x30  signature   <- 64-byte Ed25519 signature over the same body

Outputs (side-by-side with the input .bin):
  App.bin   -- patched in-place
  App.hex   -- Intel HEX with base address 0x08080000

The bootloader will refuse to boot a binary that has not been signed with
the key compiled into its ROM-resident g_rom_pubkey[].
"""

import argparse
import binascii
import getpass
import os
import struct
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Image header constants  (must match shared/include/image_header.h)
# ---------------------------------------------------------------------------
IMAGE_MAGIC         = 0x5AA5A55A
APP_BASE            = 0x08080000
APP_HDR_PADDED_SIZE = 0x200   # 512 B -- body starts here

HDR_OFF_MAGIC   = 0x00
HDR_OFF_VERSION = 0x04
HDR_OFF_LENGTH  = 0x08
HDR_OFF_CRC32   = 0x0C
HDR_OFF_PUBKEY  = 0x10      # 32 bytes
HDR_OFF_SIG     = 0x30      # 64 bytes

SCRIPT_DIR  = Path(__file__).resolve().parent
DEFAULT_KEY = SCRIPT_DIR / "private_key.pem"


def crc32_body(data: memoryview) -> int:
    """CRC-32/ISO-HDLC over the image body (matches bootloader's crc32_compute)."""
    return binascii.crc32(data) & 0xFFFF_FFFF


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Patch and sign an STM32 application image for secure boot"
    )
    parser.add_argument("bin",       help="Path to App.bin (modified in-place)")
    parser.add_argument("--key",     default=str(DEFAULT_KEY),
                        help=f"Path to encrypted private key PEM [{DEFAULT_KEY}]")
    parser.add_argument("--hex-out", default=None,
                        help="Output .hex path [<bin with .hex extension>]")
    args = parser.parse_args()

    try:
        from cryptography.hazmat.primitives.serialization import (
            Encoding, PublicFormat, load_pem_private_key,
        )
    except ImportError:
        print("ERROR: cryptography not installed.  Run: pip install -r tools/requirements.txt")
        return 1

    key_path = Path(args.key)
    if not key_path.exists():
        print(f"ERROR: private key not found: {key_path}")
        print("Run tools/gen_keys.py first.")
        return 1

    bin_path = Path(args.bin).resolve()
    hex_path = Path(args.hex_out) if args.hex_out else bin_path.with_suffix(".hex")

    if not bin_path.exists():
        print(f"ERROR: binary not found: {bin_path}")
        return 1

    # Load and decrypt private key
    passphrase = os.environ.get("SIGNING_PASSPHRASE")
    if not passphrase:
        passphrase = getpass.getpass("Enter private key passphrase: ")
    try:
        private_key = load_pem_private_key(key_path.read_bytes(), passphrase.encode())
    except Exception as exc:
        print(f"ERROR: Could not load private key: {exc}")
        return 1

    public_key   = private_key.public_key()
    pubkey_bytes = public_key.public_bytes(Encoding.Raw, PublicFormat.Raw)

    data = bytearray(bin_path.read_bytes())

    # Sanity checks
    if len(data) < APP_HDR_PADDED_SIZE + 4:
        print(f"ERROR: binary too small ({len(data)} B)")
        return 1

    magic = struct.unpack_from("<I", data, HDR_OFF_MAGIC)[0]
    if magic != IMAGE_MAGIC:
        print(f"ERROR: bad magic 0x{magic:08X} (expected 0x{IMAGE_MAGIC:08X})")
        return 1

    total_len = len(data)
    body      = memoryview(data)[APP_HDR_PADDED_SIZE:]

    # Sign body
    print(f"[sign_image] Signing {len(body)} byte body...")
    signature = private_key.sign(bytes(body))
    if len(signature) != 64:
        print(f"ERROR: unexpected signature length {len(signature)}")
        return 1

    # Patch header
    crc = crc32_body(body)
    struct.pack_into("<I", data, HDR_OFF_LENGTH, total_len)
    struct.pack_into("<I", data, HDR_OFF_CRC32,  crc)
    data[HDR_OFF_PUBKEY : HDR_OFF_PUBKEY + 32] = pubkey_bytes
    data[HDR_OFF_SIG    : HDR_OFF_SIG    + 64] = signature

    bin_path.write_bytes(data)
    print(f"[sign_image] Patched binary:  {bin_path}")
    print(f"[sign_image] length=0x{total_len:08X}  crc32=0x{crc:08X}")
    print(f"[sign_image] pubkey: {pubkey_bytes.hex()}")
    print(f"[sign_image] sig:    {signature.hex()}")

    # Generate Intel HEX
    try:
        from intelhex import IntelHex
    except ImportError:
        print("ERROR: intelhex not installed.  Run: pip install -r tools/requirements.txt")
        return 1

    ih = IntelHex()
    ih.loadbin(str(bin_path), offset=APP_BASE)
    ih.write_hex_file(str(hex_path))
    print(f"[sign_image] Intel HEX:       {hex_path}")
    print()
    print("Image signed successfully.  Flash with:  .\\flash.ps1")
    return 0


if __name__ == "__main__":
    sys.exit(main())
