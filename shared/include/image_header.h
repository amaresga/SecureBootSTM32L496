/*
 * @file image_header.h
 * @brief Application image header layout for Phase 3 authenticated boot.
 *
 * The header occupies the first 512 bytes of the application Flash region
 * (0x08080000 – 0x080801FF).  It is padded to 512 bytes so the vector table
 * that follows sits at a VTOR-aligned boundary (Cortex-M4 requires VTOR[8:0]=0
 * when the vector table holds >= 98 entries).
 *
 * Memory map:
 *  0x08080000  image_header_t  (512 B, padded)
 *  0x08080200  ISR vector table (VTOR = APP_VTOR_BASE)
 *  0x08080200+ application code / data
 *
 * Header field offsets:
 *  0x00  magic       4 B   IMAGE_MAGIC
 *  0x04  version     4 B   monotonically increasing
 *  0x08  length      4 B   total image size in bytes
 *  0x0C  crc32       4 B   CRC-32/ISO-HDLC over [APP_BODY_OFFSET..length-1]
 *  0x10  public_key 32 B   Ed25519 public key (informational; bootloader uses
 *                          ROM-resident copy compiled into its own image)
 *  0x30  signature  64 B   Ed25519 signature over [APP_BODY_OFFSET..length-1]
 *  0x70  _pad      400 B   padding to 512 B
 *
 * Both the CRC-32 and Ed25519 signature cover the image body starting at
 * APP_BODY_OFFSET (0x200 from APP_BASE), i.e. everything after the header.
 * This makes it straightforward to verify without needing to zero any fields.
 *
 * All patching is performed by tools/sign_image.py after linking.
 * Fields are zero in the raw ELF.
 */

#ifndef IMAGE_HEADER_H
#define IMAGE_HEADER_H

#include <stdint.h>

/* Magic value stored in image_header_t::magic. */
#define IMAGE_MAGIC         0x5AA5A55AUL

/* Size of the padded header region — must equal APP_VTOR_BASE - APP_BASE. */
#define APP_HDR_PADDED_SIZE 0x200U           /* 512 B */

/* Application Flash base (start of image header). */
#define APP_BASE            0x08080000UL

/* VTOR-aligned start of the application vector table. */
#define APP_VTOR_BASE       (APP_BASE + APP_HDR_PADDED_SIZE)

/* Offset within the image where the body (code+data) begins.
 * CRC-32 and Ed25519 cover [APP_BODY_OFFSET .. length-1]. */
#define APP_BODY_OFFSET     APP_HDR_PADDED_SIZE

/* Maximum valid application image size (512 KB = full Bank 2). */
#define APP_MAX_SIZE        (512U * 1024U)

/*
 * @brief Application image header.
 *
 * Placed at APP_BASE by the linker (.image_header section).
 * All fields except magic and version are zero in the raw ELF;
 * tools/sign_image.py fills them in after linking.
 */
typedef struct {
    uint32_t magic;          /**< Must equal IMAGE_MAGIC (0x5AA5A55A).              */
    uint32_t version;        /**< Monotonically increasing version number.            */
    uint32_t length;         /**< Total image size in bytes (patched post-build).    */
    uint32_t crc32;          /**< CRC-32/ISO-HDLC over [APP_BODY_OFFSET..length-1]. */
    uint8_t  public_key[32]; /**< Ed25519 public key (informational; patched).       */
    uint8_t  signature[64];  /**< Ed25519 sig over [APP_BODY_OFFSET..length-1].      */
    uint8_t  _pad[APP_HDR_PADDED_SIZE - 112U]; /* Padding to 512 B (0x70..0x1FF)   */
} __attribute__((packed, aligned(4))) image_header_t;

#endif /* IMAGE_HEADER_H */
