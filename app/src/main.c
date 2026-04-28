/// @file main.c
/// @brief Application entry point — boot, GPIO init, LED blink loop.
///
/// Boot sequence:
///  1. boot_init() configures the clock tree to 80 MHz and starts
///     the SysTick 1 ms timebase.
///  2. led_init() configures PC7 (LD1, green) as a push-pull output.
///  3. The main loop toggles LD1 every 500 ms for a 1 Hz blink.
///
/// NUCLEO-L496ZG-P user LEDs (Nucleo-144):
///  - LD1 (green) → PC7
///  - LD2 (blue)  → PB7
///  - LD3 (red)   → PB14

#include "boot.h"
#include "cmsis/stm32l496xx.h"

#define LED_PIN    7U     // PC7 = LD1 green
#define BLINK_HALF 2000U  // ms per half-period (1 s total period)

static void led_init(void) {
  // Enable GPIOC peripheral clock
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN_Msk;
  __asm volatile("dsb");

  // PC7 → General Purpose Output mode (MODER bits = 0b01)
  GPIOC->MODER &= ~(3UL << (LED_PIN * 2U));
  GPIOC->MODER |=  (1UL << (LED_PIN * 2U));

  // Push-pull output type (OTYPER bit = 0, reset default)
  GPIOC->OTYPER &= ~(1UL << LED_PIN);

  // Low speed (OSPEEDR bits = 0b00, reset default)
  GPIOC->OSPEEDR &= ~(3UL << (LED_PIN * 2U));

  // No pull-up / pull-down (PUPDR bits = 0b00, reset default)
  GPIOC->PUPDR &= ~(3UL << (LED_PIN * 2U));

  // Ensure LED starts in the off state
  GPIOC->BRR = (1UL << LED_PIN);
}

static inline void led_on(void)  { GPIOC->BSRR = (1UL << LED_PIN); }
static inline void led_off(void) { GPIOC->BRR  = (1UL << LED_PIN); }

static void delay_ms(uint32_t ms) {
  uint32_t start     = boot_get_tick();
  uint32_t last_kick = start;
  while ((boot_get_tick() - start) < ms) {
    // Kick the IWDG every 1 s so any delay length is safe
    if ((boot_get_tick() - last_kick) >= 1000U) {
      boot_kick_watchdog();
      last_kick = boot_get_tick();
    }
  }
}

int main(void) {
  boot_init();
  led_init();

  for (;;) {
    led_on();
    delay_ms(BLINK_HALF);
    led_off();
    delay_ms(BLINK_HALF);
  }
}
