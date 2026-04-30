/// @file ed25519_verify.c
/// @brief Ed25519 verification wrapper backed by Monocypher 3.1.3.

#include "ed25519_verify.h"
#include "monocypher-ed25519.h"

int ed25519_verify(const uint8_t signature[64],
                   const uint8_t public_key[32],
                   const uint8_t *message,
                   uint32_t       length) {
  return crypto_ed25519_check(signature, public_key, message, (size_t)length);
}
