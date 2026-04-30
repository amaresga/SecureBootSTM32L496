/// @file main.c  (bootloader)
/// @brief Bootloader entry point.
///
/// Execution sequence:
///  1. boot_init() — arms IWDG, validates stack sentinel, clocks to 80 MHz,
///     starts SysTick 1 ms timebase.
///  2. boot_verify_app() — checks image header magic, length, and CRC-32.
///  3. boot_jump_to_app() — validates SP/PC from the vector table and jumps.
///  4. If any step fails the bootloader enters an indefinite watchdog-kick
///     loop, allowing host-side recovery over SWD before IWDG reset.

#include "boot.h"
#include "cmsis/stm32l496xx.h"

int main(void) {
  int r = boot_init();
  if (r != BOOT_OK) {
    // Stack sentinel missing — probable overflow or tampered startup.
    // Spin until the IWDG fires and resets the device.
    for (;;) {}
  }

  // Verify the application image integrity before trusting it.
  if (boot_verify_app() != BOOT_OK) {
    // Image absent, corrupted, or invalid — kick the watchdog so a host
    // debugger can inspect state, then wait for IWDG reset.
    for (;;) {
      boot_kick_watchdog();
    }
  }

  // Attempt to jump to the validated application image.
  boot_jump_to_app();

  // boot_jump_to_app() returns only if the app vector table fails validation.
  for (;;) {
    boot_kick_watchdog();
  }
}
