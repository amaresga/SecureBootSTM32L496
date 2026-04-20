/// @file stm32l496xx.h
/// @brief Minimal peripheral register definitions for STM32L496xx.
///
/// Covers the peripherals used by the boot stage and LED blink application:
/// NVIC, SysTick, RCC, GPIO, Flash interface, and PWR.
/// Register layouts and bit-field positions follow RM0351 Rev 9.

#ifndef STM32L496XX_H
#define STM32L496XX_H

#include <stdint.h>

//===----------------------------------------------------------------------===//
// Core Cortex-M4 registers
//===----------------------------------------------------------------------===//

/// @brief Nested Vectored Interrupt Controller (NVIC) register map.
typedef struct {
  volatile uint32_t ISER[8];    ///< Interrupt Set Enable registers
           uint32_t RESERVED0[24];
  volatile uint32_t ICER[8];    ///< Interrupt Clear Enable registers
           uint32_t RESERVED1[24];
  volatile uint32_t ISPR[8];    ///< Interrupt Set Pending registers
           uint32_t RESERVED2[24];
  volatile uint32_t ICPR[8];    ///< Interrupt Clear Pending registers
           uint32_t RESERVED3[24];
  volatile uint32_t IABR[8];    ///< Interrupt Active Bit registers
           uint32_t RESERVED4[56];
  volatile uint8_t  IP[240];    ///< Interrupt Priority registers
           uint32_t RESERVED5[644];
  volatile uint32_t STIR;       ///< Software Trigger Interrupt register
} NVIC_Type;

/// @brief SysTick timer register map.
typedef struct {
  volatile uint32_t CTRL;   ///< Control and Status register
  volatile uint32_t LOAD;   ///< Reload Value register
  volatile uint32_t VAL;    ///< Current Value register
  volatile uint32_t CALIB;  ///< Calibration Value register
} SysTick_Type;

// SysTick CTRL bit masks
#define SysTick_CTRL_CLKSOURCE_Msk  (1UL << 2)  ///< Processor clock source select
#define SysTick_CTRL_TICKINT_Msk    (1UL << 1)  ///< SysTick exception request enable
#define SysTick_CTRL_ENABLE_Msk     (1UL << 0)  ///< Counter enable
#define SysTick_CTRL_COUNTFLAG_Msk  (1UL << 16) ///< Count flag (cleared on read)

// Core peripheral base addresses
#define SCS_BASE      (0xE000E000UL)
#define SysTick_BASE  (SCS_BASE + 0x0010UL)
#define NVIC_BASE     (SCS_BASE + 0x0100UL)

#define SysTick  ((SysTick_Type *) SysTick_BASE)  ///< SysTick peripheral
#define NVIC     ((NVIC_Type *)    NVIC_BASE)      ///< NVIC peripheral

//===----------------------------------------------------------------------===//
// Reset and Clock Control (RCC)
//===----------------------------------------------------------------------===//

/// @brief RCC register map (RM0351 §6.4).
typedef struct {
  volatile uint32_t CR;           ///< 0x00 Clock control
  volatile uint32_t ICSCR;        ///< 0x04 Internal clock sources calibration
  volatile uint32_t CFGR;         ///< 0x08 Clock configuration
  volatile uint32_t PLLCFGR;      ///< 0x0C PLL configuration
  volatile uint32_t PLLSAI1CFGR;  ///< 0x10 PLLSAI1 configuration
  volatile uint32_t PLLSAI2CFGR;  ///< 0x14 PLLSAI2 configuration
  volatile uint32_t CIER;         ///< 0x18 Clock interrupt enable
  volatile uint32_t CIFR;         ///< 0x1C Clock interrupt flag
  volatile uint32_t CICR;         ///< 0x20 Clock interrupt clear
           uint32_t RESERVED0;    ///< 0x24 Reserved
  volatile uint32_t AHB1RSTR;     ///< 0x28 AHB1 peripheral reset
  volatile uint32_t AHB2RSTR;     ///< 0x2C AHB2 peripheral reset
  volatile uint32_t AHB3RSTR;     ///< 0x30 AHB3 peripheral reset
           uint32_t RESERVED1;    ///< 0x34 Reserved
  volatile uint32_t APB1RSTR1;    ///< 0x38 APB1 peripheral reset 1
  volatile uint32_t APB1RSTR2;    ///< 0x3C APB1 peripheral reset 2
  volatile uint32_t APB2RSTR;     ///< 0x40 APB2 peripheral reset
           uint32_t RESERVED2;    ///< 0x44 Reserved
  volatile uint32_t AHB1ENR;      ///< 0x48 AHB1 peripheral clock enable
  volatile uint32_t AHB2ENR;      ///< 0x4C AHB2 peripheral clock enable
  volatile uint32_t AHB3ENR;      ///< 0x50 AHB3 peripheral clock enable
           uint32_t RESERVED3;    ///< 0x54 Reserved
  volatile uint32_t APB1ENR1;     ///< 0x58 APB1 peripheral clock enable 1
  volatile uint32_t APB1ENR2;     ///< 0x5C APB1 peripheral clock enable 2
  volatile uint32_t APB2ENR;      ///< 0x60 APB2 peripheral clock enable
           uint32_t RESERVED4;    ///< 0x64 Reserved
  volatile uint32_t AHB1SMENR;    ///< 0x68 AHB1 clock enable in Sleep/Stop
  volatile uint32_t AHB2SMENR;    ///< 0x6C AHB2 clock enable in Sleep/Stop
  volatile uint32_t AHB3SMENR;    ///< 0x70 AHB3 clock enable in Sleep/Stop
           uint32_t RESERVED5;    ///< 0x74 Reserved
  volatile uint32_t APB1SMENR1;   ///< 0x78 APB1 clock enable in Sleep/Stop 1
  volatile uint32_t APB1SMENR2;   ///< 0x7C APB1 clock enable in Sleep/Stop 2
  volatile uint32_t APB2SMENR;    ///< 0x80 APB2 clock enable in Sleep/Stop
           uint32_t RESERVED6;    ///< 0x84 Reserved
  volatile uint32_t CCIPR;        ///< 0x88 Peripherals independent clock selection
           uint32_t RESERVED7;    ///< 0x8C Reserved
  volatile uint32_t BDCR;         ///< 0x90 Backup domain control
  volatile uint32_t CSR;          ///< 0x94 Control/status
  volatile uint32_t CRRCR;        ///< 0x98 Clock recovery RC
  volatile uint32_t CCIPR2;       ///< 0x9C Peripherals clock selection 2
} RCC_TypeDef;

// RCC AHB2ENR — GPIO clock enable bits
#define RCC_AHB2ENR_GPIOAEN_Pos  0U
#define RCC_AHB2ENR_GPIOAEN_Msk  (1UL << RCC_AHB2ENR_GPIOAEN_Pos)  ///< GPIOA clock enable
#define RCC_AHB2ENR_GPIOBEN_Pos  1U
#define RCC_AHB2ENR_GPIOBEN_Msk  (1UL << RCC_AHB2ENR_GPIOBEN_Pos)  ///< GPIOB clock enable
#define RCC_AHB2ENR_GPIOCEN_Pos  2U
#define RCC_AHB2ENR_GPIOCEN_Msk  (1UL << RCC_AHB2ENR_GPIOCEN_Pos)  ///< GPIOC clock enable

// RCC CR — clock control bits
#define RCC_CR_MSION_Pos     0U
#define RCC_CR_MSION_Msk     (1UL << RCC_CR_MSION_Pos)       ///< MSI oscillator enable
#define RCC_CR_MSIRDY_Pos    1U
#define RCC_CR_MSIRDY_Msk    (1UL << RCC_CR_MSIRDY_Pos)      ///< MSI clock ready flag
#define RCC_CR_MSIRANGE_Pos  4U
#define RCC_CR_MSIRANGE_Msk  (0xFUL << RCC_CR_MSIRANGE_Pos)  ///< MSI frequency range
#define RCC_CR_MSIRGSEL_Pos  3U
#define RCC_CR_MSIRGSEL_Msk  (1UL << RCC_CR_MSIRGSEL_Pos)    ///< MSI range selection source

// RCC CFGR — clock configuration bits
#define RCC_CFGR_SWS_Pos  2U
#define RCC_CFGR_SWS_Msk  (0x3UL << RCC_CFGR_SWS_Pos)  ///< System clock switch status
#define RCC_CFGR_SW_Pos   0U
#define RCC_CFGR_SW_Msk   (0x3UL << RCC_CFGR_SW_Pos)   ///< System clock switch

//===----------------------------------------------------------------------===//
// General Purpose I/O (GPIO)
//===----------------------------------------------------------------------===//

/// @brief GPIO register map (RM0351 §8.4).
typedef struct {
  volatile uint32_t MODER;    ///< 0x00 Mode register
  volatile uint32_t OTYPER;   ///< 0x04 Output type register
  volatile uint32_t OSPEEDR;  ///< 0x08 Output speed register
  volatile uint32_t PUPDR;    ///< 0x0C Pull-up/pull-down register
  volatile uint32_t IDR;      ///< 0x10 Input data register
  volatile uint32_t ODR;      ///< 0x14 Output data register
  volatile uint32_t BSRR;     ///< 0x18 Bit set/reset register
  volatile uint32_t LCKR;     ///< 0x1C Configuration lock register
  volatile uint32_t AFR[2];   ///< 0x20 Alternate function registers [low, high]
  volatile uint32_t BRR;      ///< 0x28 Bit reset register
  volatile uint32_t ASCR;     ///< 0x2C Analog switch control register
} GPIO_TypeDef;

//===----------------------------------------------------------------------===//
// Flash interface
//===----------------------------------------------------------------------===//

/// @brief Flash interface register map (RM0351 §3.7).
typedef struct {
  volatile uint32_t ACR;       ///< Access control register
  volatile uint32_t PDKEYR;    ///< Power-down key register
  volatile uint32_t KEYR;      ///< Flash key register
  volatile uint32_t OPTKEYR;   ///< Option byte key register
  volatile uint32_t SR;        ///< Status register
  volatile uint32_t CR;        ///< Control register
  volatile uint32_t ECCR;      ///< ECC address register
           uint32_t RESERVED;
  volatile uint32_t OPTR;      ///< Option register
  volatile uint32_t PCROP1SR;  ///< PCROP1 area start address
  volatile uint32_t PCROP1ER;  ///< PCROP1 area end address
  volatile uint32_t WRP1AR;    ///< WRP1 area A address
  volatile uint32_t WRP1BR;    ///< WRP1 area B address
} FLASH_TypeDef;

// Flash ACR — access control bits
#define FLASH_ACR_LATENCY_Pos  0U
#define FLASH_ACR_LATENCY_Msk  (0x7UL << FLASH_ACR_LATENCY_Pos)  ///< Latency (wait states)
#define FLASH_ACR_PRFTEN_Pos   8U
#define FLASH_ACR_PRFTEN_Msk   (1UL << FLASH_ACR_PRFTEN_Pos)     ///< Prefetch buffer enable
#define FLASH_ACR_ICEN_Pos     9U
#define FLASH_ACR_ICEN_Msk     (1UL << FLASH_ACR_ICEN_Pos)       ///< Instruction cache enable
#define FLASH_ACR_DCEN_Pos     10U
#define FLASH_ACR_DCEN_Msk     (1UL << FLASH_ACR_DCEN_Pos)       ///< Data cache enable

//===----------------------------------------------------------------------===//
// Power control (PWR)
//===----------------------------------------------------------------------===//

/// @brief PWR register map (RM0351 §5.4).
typedef struct {
  volatile uint32_t CR1;    ///< Control register 1
  volatile uint32_t CR2;    ///< Control register 2
  volatile uint32_t CR3;    ///< Control register 3
  volatile uint32_t CR4;    ///< Control register 4
  volatile uint32_t SR1;    ///< Status register 1
  volatile uint32_t SR2;    ///< Status register 2
  volatile uint32_t SCR;    ///< Status clear register
           uint32_t RESERVED;
  volatile uint32_t PUCRA;  ///< Pull-up control register A
  volatile uint32_t PDCRA;  ///< Pull-down control register A
  volatile uint32_t PUCRB;  ///< Pull-up control register B
  volatile uint32_t PDCRB;  ///< Pull-down control register B
  volatile uint32_t PUCRC;  ///< Pull-up control register C
  volatile uint32_t PDCRC;  ///< Pull-down control register C
  volatile uint32_t PUCRD;  ///< Pull-up control register D
  volatile uint32_t PDCRD;  ///< Pull-down control register D
  volatile uint32_t PUCRE;  ///< Pull-up control register E
  volatile uint32_t PDCRE;  ///< Pull-down control register E
  volatile uint32_t PUCRF;  ///< Pull-up control register F
  volatile uint32_t PDCRF;  ///< Pull-down control register F
  volatile uint32_t PUCRG;  ///< Pull-up control register G
  volatile uint32_t PDCRG;  ///< Pull-down control register G
  volatile uint32_t PUCRH;  ///< Pull-up control register H
  volatile uint32_t PDCRH;  ///< Pull-down control register H
  volatile uint32_t PUCRI;  ///< Pull-up control register I
  volatile uint32_t PDCRI;  ///< Pull-down control register I
} PWR_TypeDef;

// PWR CR1 — voltage scaling bits
#define PWR_CR1_VOS_Pos   9U
#define PWR_CR1_VOS_Msk   (0x3UL << PWR_CR1_VOS_Pos)  ///< Voltage scaling range selection

// PWR SR2 — status bits
#define PWR_SR2_VOSF_Pos  10U
#define PWR_SR2_VOSF_Msk  (1UL << PWR_SR2_VOSF_Pos)   ///< Voltage scaling flag (1 = busy)

//===----------------------------------------------------------------------===//
// Peripheral base addresses
//===----------------------------------------------------------------------===//

#define PERIPH_BASE      (0x40000000UL)
#define APB1PERIPH_BASE   PERIPH_BASE
#define APB2PERIPH_BASE  (PERIPH_BASE + 0x00010000UL)
#define AHB1PERIPH_BASE  (PERIPH_BASE + 0x00020000UL)
#define AHB2PERIPH_BASE  (PERIPH_BASE + 0x08000000UL)

#define GPIOA_BASE   (AHB2PERIPH_BASE + 0x0000UL)
#define GPIOB_BASE   (AHB2PERIPH_BASE + 0x0400UL)
#define GPIOC_BASE   (AHB2PERIPH_BASE + 0x0800UL)
#define RCC_BASE     (AHB1PERIPH_BASE + 0x1000UL)
#define FLASH_BASE_  (AHB1PERIPH_BASE + 0x2000UL)
#define PWR_BASE     (APB1PERIPH_BASE + 0x7000UL)

#define GPIOA   ((GPIO_TypeDef *)  GPIOA_BASE)  ///< GPIOA peripheral
#define GPIOB   ((GPIO_TypeDef *)  GPIOB_BASE)  ///< GPIOB peripheral
#define GPIOC   ((GPIO_TypeDef *)  GPIOC_BASE)  ///< GPIOC peripheral
#define RCC     ((RCC_TypeDef *)   RCC_BASE)    ///< RCC peripheral
#define FLASH_  ((FLASH_TypeDef *) FLASH_BASE_) ///< Flash interface peripheral
#define PWR     ((PWR_TypeDef *)   PWR_BASE)    ///< PWR peripheral

#endif // STM32L496XX_H
