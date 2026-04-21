/*
 * @file boot.h
 * @brief Boot-stage initialisation API for STM32L496xx.
 *
 * Configures the clock tree (MSI → PLL @ 80 MHz), voltage scaling,
 * Flash latency, and the SysTick 1 ms timebase before handing
 * control to the application.
 */

#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

/*
 * @brief Executes the full boot stage.
 *
 * Performs the following steps in order:
 *  -# Switches SYSCLK to MSI 4 MHz as a stable reference.
 *  -# Raises CPU voltage to Range 1 (required for 80 MHz operation).
 *  -# Sets Flash latency to 4 wait states.
 *  -# Configures and starts the PLL: MSI 4 MHz × 40 ÷ 2 = 80 MHz.
 *  -# Selects the PLL as SYSCLK.
 *  -# Starts SysTick at 1 ms resolution.
 *
 * @return 0 on success.
 */
int boot_init(void);

/*
 * @brief Returns the millisecond counter driven by SysTick.
 *
 * The counter increments once per SysTick interrupt (every 1 ms after
 * @ref boot_init has been called).
 *
 * @return Elapsed milliseconds since @ref boot_init was called.
 */
uint32_t boot_get_tick(void);

#endif // BOOT_H
