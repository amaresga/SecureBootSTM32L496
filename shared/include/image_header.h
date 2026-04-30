/*
 * @file image_header.h
 * @brief Application image header layout for Phase 2 integrity verification.
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
 * CRC-32 coverage:
 *  The crc32 field covers bytes [16 .. length-1] of the binary image, i.e.
 *  everything after the four header words.  This avoids the chicken-and-egg
 *  problem of including the CRC field itself in the checksum.
 *  Algorithm: CRC-32/ISO-HDLC (poly 0xEDB88320, reflected, matches Python
 *  binascii.crc32()).
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

/* Maximum valid application image size (512 KB = full Bank 2). */
#define APP_MAX_SIZE        (512U * 1024U)

/*
 * @brief Application image header.
 *
 * Placed at APP_BASE by the linker (.image_header section).
 * The length and crc32 fields are patched by tools/patch_image.py after
 * linking; they are zero in the raw ELF.
 */
typedef struct {
    uint32_t magic;   /**< Must equal IMAGE_MAGIC (0x5AA5A55A).           */
    uint32_t version; /**< Monotonically increasing version number.         */
    uint32_t length;  /**< Total image size in bytes (patched post-build). */
    uint32_t crc32;   /**< CRC-32 over bytes [16..length-1] (patched).     */
    uint8_t  _pad[APP_HDR_PADDED_SIZE - 16U]; /* Padding to VTOR alignment */
} __attribute__((packed, aligned(4))) image_header_t;

#endif /* IMAGE_HEADER_H */
