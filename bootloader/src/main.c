/// @file main.c  (bootloader)
/// @brief Bootloader entry point.
///
/// Execution sequence:
///  1. boot_init() — arms IWDG, validates stack sentinel, clocks to 80 MHz,
///     starts SysTick 1 ms timebase.
///  2. boot_jump_to_app() — validates the application image header at
///     APP_BASE (0x08080000) and performs a clean jump.
///  3. If jump validation fails the bootloader enters an indefinite watchdog-
///     kick loop, allowing host-side recovery over SWD before IWDG reset.

#include "boot.h"
#include "cmsis/stm32l496xx.h"

int main(void) {
  int r = boot_init();
  if (r != BOOT_OK) {
    // Stack sentinel missing — probable overflow or tampered startup.
    // Spin until the IWDG fires and resets the device.
    for (;;) {}
  }

  // Attempt to jump to the validated application image.
  boot_jump_to_app();

  // boot_jump_to_app() returns only if the app header fails validation.
  // Kick the watchdog so a host debugger can inspect state before reset.
  for (;;) {
    boot_kick_watchdog();
  }
}
