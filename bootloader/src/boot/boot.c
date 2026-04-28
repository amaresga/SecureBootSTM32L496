/// @file boot.c
/// @brief Boot-stage security init, clock tree, and SysTick for STM32L496xx.

#include "boot.h"
#include "cmsis/stm32l496xx.h"

// Millisecond counter incremented by SysTick_Handler.
static volatile uint32_t s_tick_ms = 0U;

void SysTick_Handler(void) {
  s_tick_ms++;
}

uint32_t boot_get_tick(void) {
  return s_tick_ms;
}

//===----------------------------------------------------------------------===//
// Internal helpers
//===----------------------------------------------------------------------===//

// Start the Independent Watchdog with ~2 s timeout.
// Prescaler ÷64 (PR=4), reload 999: (64 × 1000) / 32 kHz = 2.000 s.
// Once started, IWDG cannot be stopped in normal run mode.
// Sequence per RM0351 §32.3.2:
//   1. START (0xCCCC) — enables LSI and activates IWDG
//   2. UNLOCK (0x5555) — allows writes to PR and RLR
//   3. Write PR and RLR
//   4. Wait for SR to clear (update complete)
//   5. RELOAD (0xAAAA) — load the new value before the first timeout
static void iwdg_init(void) {
  IWDG->KR  = IWDG_KR_START;   // start IWDG + enable LSI
  IWDG->KR  = IWDG_KR_UNLOCK;  // unlock PR and RLR for write
  IWDG->PR  = 4U;               // prescaler ÷64
  IWDG->RLR = 999U;             // reload = 999 → ~2 s timeout
  while (IWDG->SR & (IWDG_SR_PVU_Msk | IWDG_SR_RVU_Msk)) {}
  IWDG->KR  = IWDG_KR_RELOAD;  // refresh counter with new reload value
}

// Validate the stack sentinel written by startup assembly before .data/.bss
// init.  Returns 0 if intact, -1 if missing (overflow or tampering).
static int check_stack_sentinel(void) {
  // _sstack is a linker-defined absolute symbol: its *value* is the address
  // of the sentinel word.  We must use the symbol as a pointer, not take its
  // address — extern uint32_t[] is the idiomatic way to do this in C.
  extern uint32_t _sstack[];
  return (*(volatile uint32_t *)_sstack == 0xDEADC0DEUL) ? 0 : -1;
}

static void set_flash_latency(uint32_t latency) {
  uint32_t acr = FLASH_->ACR;
  acr &= ~FLASH_ACR_LATENCY_Msk;
  acr |= (latency << FLASH_ACR_LATENCY_Pos);
  // Enable prefetch buffer, instruction cache and data cache
  acr |= FLASH_ACR_PRFTEN_Msk | FLASH_ACR_ICEN_Msk | FLASH_ACR_DCEN_Msk;
  FLASH_->ACR = acr;
  // Poll until the hardware confirms the new latency
  while ((FLASH_->ACR & FLASH_ACR_LATENCY_Msk) !=
         (latency << FLASH_ACR_LATENCY_Pos)) {
    // spin
  }
}

static void set_voltage_range1(void) {
  // PWREN (bit 28) must be set before any PWR register write
  RCC->APB1ENR1 |= (1UL << 28);
  __asm volatile("dsb");

  // VOS = 0b01 → Range 1 (allows SYSCLK up to 80 MHz)
  uint32_t cr1 = PWR->CR1;
  cr1 &= ~PWR_CR1_VOS_Msk;
  cr1 |= (1UL << PWR_CR1_VOS_Pos);
  PWR->CR1 = cr1;

  // Wait for the internal regulator to settle
  while (PWR->SR2 & PWR_SR2_VOSF_Msk) {
    // spin
  }
}

// Configure PLL: MSI 4 MHz → ÷1 (PLLM) → ×40 (PLLN) → ÷2 (PLLR) = 80 MHz
static void configure_pll_80mhz(void) {
  RCC->CR &= ~(1UL << 24); // PLLON = 0
  while (RCC->CR & (1UL << 25)) {
    // wait PLLRDY = 0
  }

  // PLLSRC = MSI (01), PLLM = ÷1 (0), PLLN = 40, PLLR = ÷2 (0), PLLREN = 1
  RCC->PLLCFGR = (1UL  <<  0) |  // PLLSRC = MSI
                 (0UL  <<  4) |  // PLLM = 0 → ÷1
                 (40UL <<  8) |  // PLLN = 40
                 (0UL  << 25) |  // PLLR = 0 → ÷2
                 (1UL  << 24);   // PLLREN

  RCC->CR |= (1UL << 24); // PLLON = 1
  while (!(RCC->CR & (1UL << 25))) {
    // wait PLLRDY = 1
  }
}

//===----------------------------------------------------------------------===//
// Public API
//===----------------------------------------------------------------------===//

void boot_kick_watchdog(void) {
  IWDG->KR = IWDG_KR_RELOAD;
}

uint8_t boot_get_rdp_level(void) {
  uint32_t rdp = FLASH_->OPTR & FLASH_OPTR_RDP_Msk;
  if (rdp == FLASH_OPTR_RDP_LEVEL0) return 0U;
  if (rdp == FLASH_OPTR_RDP_LEVEL2) return 2U;
  return 1U;
}

int boot_init(void) {
  // 1. Arm the independent watchdog immediately — catches any hang that
  //    follows, including in the clock-switch sequence.
  iwdg_init();

  // 2. Verify the stack sentinel written by startup assembly.
  //    A missing sentinel means either the stack overflowed before the
  //    first C instruction or the startup code was bypassed.
  if (check_stack_sentinel() != 0) {
    return BOOT_ERR_STACK_CORRUPT;
  }

  // 3. Ensure MSI is running and stable at 4 MHz (MSIRANGE = 6)
  RCC->CR |= RCC_CR_MSION_Msk;
  while (!(RCC->CR & RCC_CR_MSIRDY_Msk)) {
    // spin
  }

  uint32_t cr = RCC->CR;
  cr &= ~RCC_CR_MSIRANGE_Msk;
  cr |= (6UL << RCC_CR_MSIRANGE_Pos); // 4 MHz
  cr |=  RCC_CR_MSIRGSEL_Msk;         // use MSIRANGE in CR (not RTC)
  RCC->CR = cr;
  while (!(RCC->CR & RCC_CR_MSIRDY_Msk)) {
    // spin
  }

  // Switch SYSCLK to MSI while reconfiguring the PLL
  RCC->CFGR &= ~RCC_CFGR_SW_Msk; // SW = 0b00 → MSI
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (0UL << RCC_CFGR_SWS_Pos)) {
    // spin
  }

  // 4. Raise CPU voltage to Range 1 (required for frequencies above 26 MHz)
  set_voltage_range1();

  // 5. Set Flash latency to 4 WS (required at 80 MHz, VOS Range 1, 3.3 V)
  set_flash_latency(4U);

  // 6. Configure and start the PLL → 80 MHz
  configure_pll_80mhz();

  // 7. Select PLL as SYSCLK
  uint32_t cfgr = RCC->CFGR;
  cfgr &= ~RCC_CFGR_SW_Msk;
  cfgr |= (3UL << RCC_CFGR_SW_Pos); // SW = 0b11 → PLL
  RCC->CFGR = cfgr;
  while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (3UL << RCC_CFGR_SWS_Pos)) {
    // spin
  }

  // 8. Configure SysTick: 80 000 ticks at 80 MHz = exactly 1 ms period
  SysTick->LOAD = 80000U - 1U;
  SysTick->VAL  = 0U;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk  // processor clock source
                | SysTick_CTRL_TICKINT_Msk     // generate exception on reload
                | SysTick_CTRL_ENABLE_Msk;     // start counter

  return BOOT_OK;
}

void boot_jump_to_app(void) {
  /* word 0 = initial SP, word 1 = reset vector (Thumb bit set) */
  const uint32_t *app_hdr = (const uint32_t *)APP_BASE;
  uint32_t app_sp = app_hdr[0];
  uint32_t app_pc = app_hdr[1];

  // SP must be within SRAM1 (256 KB at 0x20000000)
  if (app_sp < 0x20000000UL || app_sp > 0x20040000UL) {
    return; // invalid — stay in bootloader
  }

  // Reset vector must be inside app Flash range and have Thumb bit set
  uint32_t app_pc_aligned = app_pc & ~1UL;
  if (app_pc_aligned < APP_BASE || app_pc_aligned >= (APP_BASE + 512U * 1024U)) {
    return; // invalid — stay in bootloader
  }

  // Disable all interrupts before changing execution context
  __asm volatile("cpsid i" ::: "memory");

  // Stop SysTick so the app starts with a clean timer state
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL  = 0U;

  // Relocate the vector table to the application base
  SCB->VTOR = APP_BASE;

  // Load the application's initial stack pointer into MSP
  __asm volatile("msr msp, %0" :: "r"(app_sp) : "memory");

  // Re-enable interrupts now that VTOR and MSP are fully committed.
  // CPSID I was set above to guard the VTOR/MSP transition; the app
  // depends on interrupts (SysTick) being active from the start.
  __asm volatile("cpsie i" ::: "memory");

  // Branch to the application reset handler — does not return
  typedef void (*reset_fn_t)(void);
  ((reset_fn_t)app_pc)();

  // Unreachable; belt-and-suspenders infinite loop
  for (;;) {}
}
