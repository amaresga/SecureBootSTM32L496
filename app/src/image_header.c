/// @file image_header.c
/// @brief Application image header placed at APP_BASE (0x08080000).
///
/// The length, crc32, public_key, and signature fields are zero here;
/// tools/sign_image.py fills them all in after linking.

#include "image_header.h"

__attribute__((section(".image_header"), used))
const image_header_t g_image_header = {
  .magic      = IMAGE_MAGIC,
  .version    = 1U,
  .length     = 0U,    /* patched by tools/sign_image.py */
  .crc32      = 0U,    /* patched by tools/sign_image.py */
  .public_key = {0},   /* patched by tools/sign_image.py */
  .signature  = {0},   /* patched by tools/sign_image.py */
};
