/*
 * @file boot.h
 * @brief Boot-stage initialisation API for STM32L496xx.
 *
 * Performs the full secure boot sequence: arms the IWDG, validates the
 * stack sentinel, configures the clock tree (MSI → PLL @ 80 MHz), raises
 * voltage, sets Flash latency, and starts the SysTick 1 ms timebase.
 */

#ifndef BOOT_H
#define BOOT_H

#include <stdint.h>

// Boot return codes
#define BOOT_OK                  0   /**< Boot stage completed successfully */
#define BOOT_ERR_STACK_CORRUPT  (-1) /**< Stack sentinel absent — possible overflow or tampering */

/*
 * @brief Executes the full boot stage.
 *
 * Performs the following steps in order:
 *  -# Arms the Independent Watchdog (~2 s timeout).
 *  -# Validates the stack sentinel written by startup assembly.
 *  -# Switches SYSCLK to MSI 4 MHz as a stable reference.
 *  -# Raises CPU voltage to Range 1 (required for 80 MHz operation).
 *  -# Sets Flash latency to 4 wait states.
 *  -# Configures and starts the PLL: MSI 4 MHz × 40 ÷ 2 = 80 MHz.
 *  -# Selects the PLL as SYSCLK.
 *  -# Starts SysTick at 1 ms resolution.
 *
 * @return BOOT_OK on success, BOOT_ERR_STACK_CORRUPT if the sentinel is missing.
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

/*
 * @brief Reloads the Independent Watchdog counter.
 *
 * Must be called within the watchdog timeout period (~2 s) after
 * @ref boot_init returns, or the device will perform a watchdog reset.
 * Call this at regular intervals inside the main application loop.
 */
void boot_kick_watchdog(void);

/*
 * @brief Returns the current Flash read-out protection level.
 *
 * Reads the RDP byte from the Flash option register (OPTR).
 * In a production secure-boot environment the level should be >= 1.
 *
 * @return 0 = Level 0 (no protection), 1 = Level 1, 2 = Level 2 (permanent).
 */
uint8_t boot_get_rdp_level(void);

#endif // BOOT_H
