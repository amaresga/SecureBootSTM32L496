/// @file image_header.c
/// @brief Application image header placed at APP_BASE (0x08080000).
///
/// The length and crc32 fields are zero here; tools/patch_image.py fills
/// them in after linking by computing the image size and CRC-32 from the
/// generated binary.

#include "image_header.h"

__attribute__((section(".image_header"), used))
const image_header_t g_image_header = {
  .magic   = IMAGE_MAGIC,
  .version = 1U,
  .length  = 0U, /* patched by tools/patch_image.py */
  .crc32   = 0U, /* patched by tools/patch_image.py */
};
