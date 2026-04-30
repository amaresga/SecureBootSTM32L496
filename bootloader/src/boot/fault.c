/*
 * @file fault.c
 * @brief Cortex-M4 fault exception handlers for STM32L496xx.
 *
 * Implements HardFault, BusFault, UsageFault, MemManage, and NMI handlers.
 * Each handler captures the exception stack frame and SCB fault registers
 * into g_fault_record for post-mortem analysis, then either:
 *
 *  - Release builds (NDEBUG defined): issues an immediate system reset via
 *    SCB AIRCR.SYSRESETREQ so the device recovers automatically.
 *  - Debug builds: halts in an infinite loop so a JTAG/SWD debugger can
 *    inspect g_fault_record and the register dump.
 *
 * To retain g_fault_record across a soft reset (e.g. for a "last fault"
 * log), move the struct to a .noinit section placed in SRAM2 @ 0x10000000
 * and exclude that region from the SRAM scrub in startup_stm32l496xx.s.
 */

#include "cmsis/stm32l496xx.h"
#include <stdint.h>

//===----------------------------------------------------------------------===//
// Fault record — populated before reset/halt
//===----------------------------------------------------------------------===//

/*
 * @brief Snapshot of CPU state at the time of the fault.
 *
 * The first eight fields mirror the exception stack frame that the
 * Cortex-M4 hardware pushes automatically before entering the handler.
 * The remaining fields are read from the SCB fault status registers.
 */
typedef struct {
  uint32_t r0;    // Stacked R0
  uint32_t r1;    // Stacked R1
  uint32_t r2;    // Stacked R2
  uint32_t r3;    // Stacked R3
  uint32_t r12;   // Stacked R12
  uint32_t lr;    // Stacked Link Register (return address)
  uint32_t pc;    // Stacked PC — address of the faulting instruction
  uint32_t xpsr;  // Stacked xPSR
  uint32_t cfsr;  // Configurable Fault Status (MMFSR + BFSR + UFSR)
  uint32_t hfsr;  // HardFault Status
  uint32_t mmfar; // MemManage fault address (valid when CFSR.MMARVALID)
  uint32_t bfar;  // BusFault address (valid when CFSR.BFARVALID)
} FaultRecord;

volatile FaultRecord g_fault_record;

//===----------------------------------------------------------------------===//
// Shared C-level handler (called from all four naked entries below)
//===----------------------------------------------------------------------===//

static void __attribute__((noreturn, used))
fault_record_and_reset(const volatile uint32_t *frame)
{
  g_fault_record.r0    = frame[0];
  g_fault_record.r1    = frame[1];
  g_fault_record.r2    = frame[2];
  g_fault_record.r3    = frame[3];
  g_fault_record.r12   = frame[4];
  g_fault_record.lr    = frame[5];
  g_fault_record.pc    = frame[6];
  g_fault_record.xpsr  = frame[7];
  g_fault_record.cfsr  = SCB->CFSR;
  g_fault_record.hfsr  = SCB->HFSR;
  g_fault_record.mmfar = SCB->MMFAR;
  g_fault_record.bfar  = SCB->BFAR;

#if defined(NDEBUG)
  // Production: reset the device immediately so it can recover.
  SCB->AIRCR = SCB_AIRCR_VECTKEY_Val | SCB_AIRCR_SYSRESETREQ_Msk;
  __asm volatile("dsb");
#endif
  // Debug: halt here — connect GDB and inspect g_fault_record.pc,
  // g_fault_record.cfsr, g_fault_record.hfsr for the root cause.
  for (;;) {}
}

//===----------------------------------------------------------------------===//
// ARM exception entries
// Each handler is naked (no compiler prologue) so the hardware-stacked
// frame at MSP or PSP is still intact when we read it.
//===----------------------------------------------------------------------===//

// Select MSP or PSP depending on the EXC_RETURN value in LR, then
// branch to the C handler with the frame pointer in R0.
#define FAULT_ENTRY()                                \
  __asm volatile(                                    \
    "tst   lr, #4                \n"                 \
    "ite   eq                    \n"                 \
    "mrseq r0, msp               \n"                 \
    "mrsne r0, psp               \n"                 \
    "b     fault_record_and_reset\n"                 \
  )

__attribute__((naked)) void HardFault_Handler(void)  { FAULT_ENTRY(); }
__attribute__((naked)) void MemManage_Handler(void)  { FAULT_ENTRY(); }
__attribute__((naked)) void BusFault_Handler(void)   { FAULT_ENTRY(); }
__attribute__((naked)) void UsageFault_Handler(void) { FAULT_ENTRY(); }

//===----------------------------------------------------------------------===//
// NMI — issued by the Clock Security System if HSE fails
//===----------------------------------------------------------------------===//

void NMI_Handler(void) {
  // NMI is triggered by CSS when HSE oscillator failure is detected.
  // Reset immediately; the application will reinitialise on MSI.
  SCB->AIRCR = SCB_AIRCR_VECTKEY_Val | SCB_AIRCR_SYSRESETREQ_Msk;
  __asm volatile("dsb");
  for (;;) {}
}
