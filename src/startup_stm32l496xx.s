/**
 * @file startup_stm32l496xx.s
 * @brief Startup file for STM32L496xx — sets up stack, copies .data,
 *        zeroes .bss, then calls SystemInit → main.
 */

    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

/* Stack pointer value defined in linker script */
    .global _estack

/* External symbols provided by linker script */
    .global _sidata
    .global _sdata
    .global _edata
    .global _sbss
    .global _ebss

    .global Reset_Handler
    .global Default_Handler

/* Weak references to libc init / C++ constructors */
    .weak  __libc_init_array

/*===========================================================================
 * Vector table
 *=========================================================================*/
    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object

g_pfnVectors:
    .word _estack                     /* Initial stack pointer          */
    .word Reset_Handler               /* Reset                          */
    .word NMI_Handler                 /* NMI                            */
    .word HardFault_Handler           /* Hard Fault                     */
    .word MemManage_Handler           /* MPU Fault                      */
    .word BusFault_Handler            /* Bus Fault                      */
    .word UsageFault_Handler          /* Usage Fault                    */
    .word 0                           /* Reserved                       */
    .word 0                           /* Reserved                       */
    .word 0                           /* Reserved                       */
    .word 0                           /* Reserved                       */
    .word SVC_Handler                 /* SVCall                         */
    .word DebugMon_Handler            /* Debug Monitor                  */
    .word 0                           /* Reserved                       */
    .word PendSV_Handler              /* PendSV                         */
    .word SysTick_Handler             /* SysTick                        */

    /* External interrupts (IRQ0 – IRQ81 for STM32L496) */
    .word WWDG_IRQHandler
    .word PVD_PVM_IRQHandler
    .word TAMP_STAMP_IRQHandler
    .word RTC_WKUP_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word CAN1_TX_IRQHandler
    .word CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_TIM15_IRQHandler
    .word TIM1_UP_TIM16_IRQHandler
    .word TIM1_TRG_COM_TIM17_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word DFSDM1_FLT3_IRQHandler
    .word TIM8_BRK_IRQHandler
    .word TIM8_UP_IRQHandler
    .word TIM8_TRG_COM_IRQHandler
    .word TIM8_CC_IRQHandler
    .word ADC3_IRQHandler
    .word FMC_IRQHandler
    .word SDMMC1_IRQHandler
    .word TIM5_IRQHandler
    .word SPI3_IRQHandler
    .word UART4_IRQHandler
    .word UART5_IRQHandler
    .word TIM6_DAC_IRQHandler
    .word TIM7_IRQHandler
    .word DMA2_Channel1_IRQHandler
    .word DMA2_Channel2_IRQHandler
    .word DMA2_Channel3_IRQHandler
    .word DMA2_Channel4_IRQHandler
    .word DMA2_Channel5_IRQHandler
    .word DFSDM1_FLT0_IRQHandler
    .word DFSDM1_FLT1_IRQHandler
    .word DFSDM1_FLT2_IRQHandler
    .word COMP_IRQHandler
    .word LPTIM1_IRQHandler
    .word LPTIM2_IRQHandler
    .word OTG_FS_IRQHandler
    .word DMA2_Channel6_IRQHandler
    .word DMA2_Channel7_IRQHandler
    .word LPUART1_IRQHandler
    .word QUADSPI_IRQHandler
    .word I2C3_EV_IRQHandler
    .word I2C3_ER_IRQHandler
    .word SAI1_IRQHandler
    .word SAI2_IRQHandler
    .word SWPMI1_IRQHandler
    .word TSC_IRQHandler
    .word LCD_IRQHandler
    .word AES_IRQHandler
    .word RNG_IRQHandler
    .word FPU_IRQHandler
    .word CRS_IRQHandler
    .word I2C4_EV_IRQHandler
    .word I2C4_ER_IRQHandler
    .word DCMI_IRQHandler
    .word CAN2_TX_IRQHandler
    .word CAN2_RX0_IRQHandler
    .word CAN2_RX1_IRQHandler
    .word CAN2_SCE_IRQHandler
    .word DMA2D_IRQHandler

/*===========================================================================
 * Reset handler
 *=========================================================================*/
    .section .text.Reset_Handler
    .weak  Reset_Handler
    .type  Reset_Handler, %function

Reset_Handler:
    /* Set stack pointer explicitly (required for some debug scenarios) */
    ldr   sp, =_estack

    /* Copy initialised data from Flash (.sidata) to RAM (.data) */
    movs  r1, #0
    b     LoopCopyDataInit

CopyDataInit:
    ldr   r3, =_sidata
    ldr   r3, [r3, r1]
    str   r3, [r0, r1]
    adds  r1, r1, #4

LoopCopyDataInit:
    ldr   r0, =_sdata
    ldr   r3, =_edata
    adds  r2, r0, r1
    cmp   r2, r3
    bcc   CopyDataInit

    /* Zero .bss */
    ldr   r2, =_sbss
    b     LoopFillZerobss

FillZerobss:
    movs  r3, #0
    str   r3, [r2], #4

LoopFillZerobss:
    ldr   r3, =_ebss
    cmp   r2, r3
    bcc   FillZerobss

    /* Call SystemInit (clock setup) */
    bl    SystemInit

    /* Call C++ constructors / libc init */
    bl    __libc_init_array

    /* Enter application */
    bl    main

    /* Should never reach here — loop forever */
LoopForever:
    b     LoopForever

    .size Reset_Handler, .-Reset_Handler

/*===========================================================================
 * Default handler (infinite loop) — all unhandled IRQs alias here
 *=========================================================================*/
    .section .text.Default_Handler, "ax", %progbits

Default_Handler:
Infinite_Loop:
    b Infinite_Loop
    .size Default_Handler, .-Default_Handler

/*---------------------------------------------------------------------------
 * Weak aliases — override any of these in your application code
 *---------------------------------------------------------------------------*/
    .macro WEAK_IRQ name
    .weak  \name
    .thumb_set \name, Default_Handler
    .endm

    WEAK_IRQ NMI_Handler
    WEAK_IRQ HardFault_Handler
    WEAK_IRQ MemManage_Handler
    WEAK_IRQ BusFault_Handler
    WEAK_IRQ UsageFault_Handler
    WEAK_IRQ SVC_Handler
    WEAK_IRQ DebugMon_Handler
    WEAK_IRQ PendSV_Handler
    WEAK_IRQ SysTick_Handler
    WEAK_IRQ WWDG_IRQHandler
    WEAK_IRQ PVD_PVM_IRQHandler
    WEAK_IRQ TAMP_STAMP_IRQHandler
    WEAK_IRQ RTC_WKUP_IRQHandler
    WEAK_IRQ FLASH_IRQHandler
    WEAK_IRQ RCC_IRQHandler
    WEAK_IRQ EXTI0_IRQHandler
    WEAK_IRQ EXTI1_IRQHandler
    WEAK_IRQ EXTI2_IRQHandler
    WEAK_IRQ EXTI3_IRQHandler
    WEAK_IRQ EXTI4_IRQHandler
    WEAK_IRQ DMA1_Channel1_IRQHandler
    WEAK_IRQ DMA1_Channel2_IRQHandler
    WEAK_IRQ DMA1_Channel3_IRQHandler
    WEAK_IRQ DMA1_Channel4_IRQHandler
    WEAK_IRQ DMA1_Channel5_IRQHandler
    WEAK_IRQ DMA1_Channel6_IRQHandler
    WEAK_IRQ DMA1_Channel7_IRQHandler
    WEAK_IRQ ADC1_2_IRQHandler
    WEAK_IRQ CAN1_TX_IRQHandler
    WEAK_IRQ CAN1_RX0_IRQHandler
    WEAK_IRQ CAN1_RX1_IRQHandler
    WEAK_IRQ CAN1_SCE_IRQHandler
    WEAK_IRQ EXTI9_5_IRQHandler
    WEAK_IRQ TIM1_BRK_TIM15_IRQHandler
    WEAK_IRQ TIM1_UP_TIM16_IRQHandler
    WEAK_IRQ TIM1_TRG_COM_TIM17_IRQHandler
    WEAK_IRQ TIM1_CC_IRQHandler
    WEAK_IRQ TIM2_IRQHandler
    WEAK_IRQ TIM3_IRQHandler
    WEAK_IRQ TIM4_IRQHandler
    WEAK_IRQ I2C1_EV_IRQHandler
    WEAK_IRQ I2C1_ER_IRQHandler
    WEAK_IRQ I2C2_EV_IRQHandler
    WEAK_IRQ I2C2_ER_IRQHandler
    WEAK_IRQ SPI1_IRQHandler
    WEAK_IRQ SPI2_IRQHandler
    WEAK_IRQ USART1_IRQHandler
    WEAK_IRQ USART2_IRQHandler
    WEAK_IRQ USART3_IRQHandler
    WEAK_IRQ EXTI15_10_IRQHandler
    WEAK_IRQ RTC_Alarm_IRQHandler
    WEAK_IRQ DFSDM1_FLT3_IRQHandler
    WEAK_IRQ TIM8_BRK_IRQHandler
    WEAK_IRQ TIM8_UP_IRQHandler
    WEAK_IRQ TIM8_TRG_COM_IRQHandler
    WEAK_IRQ TIM8_CC_IRQHandler
    WEAK_IRQ ADC3_IRQHandler
    WEAK_IRQ FMC_IRQHandler
    WEAK_IRQ SDMMC1_IRQHandler
    WEAK_IRQ TIM5_IRQHandler
    WEAK_IRQ SPI3_IRQHandler
    WEAK_IRQ UART4_IRQHandler
    WEAK_IRQ UART5_IRQHandler
    WEAK_IRQ TIM6_DAC_IRQHandler
    WEAK_IRQ TIM7_IRQHandler
    WEAK_IRQ DMA2_Channel1_IRQHandler
    WEAK_IRQ DMA2_Channel2_IRQHandler
    WEAK_IRQ DMA2_Channel3_IRQHandler
    WEAK_IRQ DMA2_Channel4_IRQHandler
    WEAK_IRQ DMA2_Channel5_IRQHandler
    WEAK_IRQ DFSDM1_FLT0_IRQHandler
    WEAK_IRQ DFSDM1_FLT1_IRQHandler
    WEAK_IRQ DFSDM1_FLT2_IRQHandler
    WEAK_IRQ COMP_IRQHandler
    WEAK_IRQ LPTIM1_IRQHandler
    WEAK_IRQ LPTIM2_IRQHandler
    WEAK_IRQ OTG_FS_IRQHandler
    WEAK_IRQ DMA2_Channel6_IRQHandler
    WEAK_IRQ DMA2_Channel7_IRQHandler
    WEAK_IRQ LPUART1_IRQHandler
    WEAK_IRQ QUADSPI_IRQHandler
    WEAK_IRQ I2C3_EV_IRQHandler
    WEAK_IRQ I2C3_ER_IRQHandler
    WEAK_IRQ SAI1_IRQHandler
    WEAK_IRQ SAI2_IRQHandler
    WEAK_IRQ SWPMI1_IRQHandler
    WEAK_IRQ TSC_IRQHandler
    WEAK_IRQ LCD_IRQHandler
    WEAK_IRQ AES_IRQHandler
    WEAK_IRQ RNG_IRQHandler
    WEAK_IRQ FPU_IRQHandler
    WEAK_IRQ CRS_IRQHandler
    WEAK_IRQ I2C4_EV_IRQHandler
    WEAK_IRQ I2C4_ER_IRQHandler
    WEAK_IRQ DCMI_IRQHandler
    WEAK_IRQ CAN2_TX_IRQHandler
    WEAK_IRQ CAN2_RX0_IRQHandler
    WEAK_IRQ CAN2_RX1_IRQHandler
    WEAK_IRQ CAN2_SCE_IRQHandler
    WEAK_IRQ DMA2D_IRQHandler
