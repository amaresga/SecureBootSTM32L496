/*
 * @file pubkey.h
 * @brief ROM-resident Ed25519 public key used by the bootloader to verify
 *        the application image signature.
 *
 * This key is compiled into the bootloader image and is intentionally NOT
 * taken from the application image header — the header copy is untrusted.
 *
 * Key rotation requires re-generating keys with tools/gen_keys.py, updating
 * this file (done automatically by gen_keys.py), rebuilding the bootloader,
 * and reflashing it.
 */

#ifndef PUBKEY_H
#define PUBKEY_H

#include <stdint.h>

/* ROM-resident Ed25519 public key (32 bytes). Populated by gen_keys.py. */
extern const uint8_t g_rom_pubkey[32];

#endif /* PUBKEY_H */
