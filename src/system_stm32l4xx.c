/// @file system_stm32l4xx.c
/// @brief SystemInit() implementation for STM32L496xx.
///
/// Called by Reset_Handler before main(). Configures the SCB for maximum
/// fault visibility, sets the vector table offset, and enables the FPU.
/// Clock configuration is intentionally deferred to boot_init() so it
/// can run from C with proper register access.

#include <stdint.h>
#include "cmsis/stm32l496xx.h"

// SystemCoreClock holds the current SYSCLK frequency in Hz.
// Initialised to 4 MHz (MSI default at reset); updated by boot_init().
uint32_t SystemCoreClock = 4000000U;

void SystemInit(void) {
  // 1. Set VTOR to the start of Flash so the vector table cannot be
  //    redirected to RAM by a pre-boot attacker.
  SCB->VTOR = 0x08000000UL;

  // 2. Enable configurable fault exceptions so BusFault, UsageFault,
  //    and MemManage are routed to their dedicated handlers instead of
  //    escalating silently to HardFault.
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
              | SCB_SHCSR_BUSFAULTENA_Msk
              | SCB_SHCSR_MEMFAULTENA_Msk;

  // 3. Trap divide-by-zero as a UsageFault (catches C division bugs
  //    that would otherwise produce silent wrong results on Cortex-M4).
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;

  // 4. Enable the FPU coprocessors (CP10 + CP11 → full access).
#if defined(__FPU_USED) && (__FPU_USED == 1)
  SCB->CPACR |= (0xFUL << 20U);
  __asm volatile("dsb"); // ensure CPACR write completes
  __asm volatile("isb"); // flush pipeline so new FPU state takes effect
#endif
}
