/*
 * @file ed25519_verify.h
 * @brief Thin wrapper around Monocypher's Ed25519 verification for the bootloader.
 *
 * Provides a stable interface so the underlying crypto library can be swapped
 * (e.g. to X-CUBE-CRYPTOLIB or hardware-assisted PKA) without touching boot.c.
 */

#ifndef ED25519_VERIFY_H
#define ED25519_VERIFY_H

#include <stdint.h>

/*
 * @brief Verify an Ed25519 signature.
 *
 * @param signature   64-byte Ed25519 signature.
 * @param public_key  32-byte Ed25519 public key.
 * @param message     Pointer to the signed message body.
 * @param length      Length of the message body in bytes.
 *
 * @return 0 if the signature is valid, -1 otherwise.
 */
int ed25519_verify(const uint8_t signature[64],
                   const uint8_t public_key[32],
                   const uint8_t *message,
                   uint32_t       length);

#endif /* ED25519_VERIFY_H */
