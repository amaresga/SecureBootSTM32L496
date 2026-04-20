/// @file system_stm32l4xx.c
/// @brief SystemInit() implementation for STM32L496xx.
///
/// Called by Reset_Handler before main(). Enables the FPU coprocessor.
/// Clock configuration is intentionally deferred to boot_init() so it
/// can run from C with proper register access.

#include <stdint.h>

// SystemCoreClock holds the current SYSCLK frequency in Hz.
// Initialised to 4 MHz (MSI default at reset); updated by boot_init().
uint32_t SystemCoreClock = 4000000U;

void SystemInit(void) {
  // Full clock init is deferred to boot_init().
  // Enable the FPU here so it is available before main() runs.
#if defined(__FPU_USED) && (__FPU_USED == 1)
  // Set CP10 and CP11 (FPU coprocessors) to full access in CPACR
  *((volatile uint32_t *)0xE000ED88U) |= (0xFUL << 20);
  __asm volatile("dsb"); // ensure CPACR write completes
  __asm volatile("isb"); // flush pipeline so new FPU state takes effect
#endif
}
