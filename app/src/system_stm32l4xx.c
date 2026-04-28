/// @file system_stm32l4xx.c
/// @brief SystemInit() for the application image at 0x08080000.
///
/// Called by Reset_Handler before main(). Sets VTOR to the application Flash
/// base (Bank 2), enables configurable fault exceptions, traps divide-by-zero,
/// and enables the FPU.  Clock configuration is deferred to boot_init().

#include <stdint.h>
#include "cmsis/stm32l496xx.h"

// SystemCoreClock holds the current SYSCLK frequency in Hz.
// Initialised to 4 MHz (MSI default at reset); updated by boot_init().
uint32_t SystemCoreClock = 4000000U;

void SystemInit(void) {
  // 1. Set VTOR to the application Flash base so the vector table cannot be
  //    redirected by an earlier-stage attacker.
  SCB->VTOR = 0x08080000UL;

  // 2. Enable configurable fault exceptions so BusFault, UsageFault,
  //    and MemManage are routed to their dedicated handlers instead of
  //    escalating silently to HardFault.
  SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
              | SCB_SHCSR_BUSFAULTENA_Msk
              | SCB_SHCSR_MEMFAULTENA_Msk;

  // 3. Trap divide-by-zero as a UsageFault.
  SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;

  // 4. Enable the FPU coprocessors (CP10 + CP11 → full access).
#if defined(__FPU_USED) && (__FPU_USED == 1)
  SCB->CPACR |= (0xFUL << 20U);
  __asm volatile("dsb"); // ensure CPACR write completes
  __asm volatile("isb"); // flush pipeline so new FPU state takes effect
#endif
}
