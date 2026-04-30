/// @file verify.c
/// @brief Application image integrity and authenticity verification.
///
/// This file is compiled only into the bootloader (not the application).
/// It performs:
///   1. CRC-32/ISO-HDLC pre-check over the image body (fast rejection)
///   2. Ed25519 signature verification using the ROM-resident public key
///
/// Coverage for both checks: [APP_BODY_OFFSET .. length-1] — i.e. everything
/// after the 512-byte header.  The header itself (magic, version, length,
/// crc32, public_key, signature, _pad) is checked structurally; its public_key
/// field is informational only and is never trusted for cryptographic purposes.

#include "boot.h"
#include "image_header.h"
#include "ed25519_verify.h"
#include "pubkey.h"

/* CRC-32/ISO-HDLC — reflected poly 0xEDB88320, init/xor 0xFFFFFFFF.
 * Matches Python binascii.crc32() and tools/sign_image.py. */
static uint32_t crc32_compute(const uint8_t *data, uint32_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t i = 0U; i < len; i++) {
    crc ^= (uint32_t)data[i];
    for (int b = 0; b < 8; b++) {
      if (crc & 1U) {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

int boot_verify_app(void) {
  const image_header_t *hdr = (const image_header_t *)APP_BASE;

  // 1. Check magic
  if (hdr->magic != IMAGE_MAGIC) {
    return BOOT_ERR_APP_INVALID;
  }

  // 2. Length must exceed the header and fit in the app Flash region
  if (hdr->length <= APP_BODY_OFFSET || hdr->length > APP_MAX_SIZE) {
    return BOOT_ERR_APP_INVALID;
  }

  // Image body: everything after the 512-byte header
  const uint8_t *body     = (const uint8_t *)APP_BASE + APP_BODY_OFFSET;
  uint32_t       body_len = hdr->length - APP_BODY_OFFSET;

  // 3. Fast CRC-32 pre-check
  uint32_t computed = crc32_compute(body, body_len);
  if (computed != hdr->crc32) {
    return BOOT_ERR_APP_INVALID;
  }

  // 4. Ed25519 signature — ROM-resident key only; header copy is untrusted
  if (ed25519_verify(hdr->signature, g_rom_pubkey, body, body_len) != 0) {
    return BOOT_ERR_APP_INVALID;
  }

  return BOOT_OK;
}
