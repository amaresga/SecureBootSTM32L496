# STM32L496 Boot Process

This document describes every step from power-on to the first line of `main()`,
and what each stage initialises and why.

---

## Overview

```
Power-on / Reset
      │
      ▼
┌─────────────────────────────┐
│  Stage 0 — Hardware Reset   │  (Cortex-M4 core, done automatically)
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│  Stage 1 — Reset_Handler    │  startup_stm32l496xx.s
│  • Set stack pointer        │
│  • Copy .data Flash → RAM   │
│  • Zero .bss                │
│  • Call SystemInit()        │
│  • Call __libc_init_array() │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│  Stage 2 — SystemInit()     │  system_stm32l4xx.c
│  • Enable FPU               │
└─────────────┬───────────────┘
              │
              ▼
┌─────────────────────────────┐
│  Stage 3 — main()           │  src/main.c
│  • Call boot_init()  ──────►│  src/boot/boot.c
│    • MSI → 4 MHz            │
│    • Voltage Range 1        │
│    • Flash latency 4 WS     │
│    • PLL → 80 MHz SYSCLK    │
│    • SysTick 1 ms           │
│  • led_init()               │
│  • Blink loop               │
└─────────────────────────────┘
```

---

## Stage 0 — Hardware Reset (Cortex-M4 core)

**Who does it:** The silicon itself — no code involved.

When the MCU comes out of reset the Cortex-M4 hardware performs two automatic
actions before executing a single instruction:

| Action | Source | Why |
|--------|--------|-----|
| Load the **initial Stack Pointer** | `0x08000000` word 0 (`_estack`) | The core needs a valid SP before it can push/pop anything — including calling functions |
| Load the **Program Counter** | `0x08000000` word 1 (`Reset_Handler`) | Tells the core where to start executing |

Both values are the first two entries of the **vector table** placed at the
start of Flash by the linker script.

---

## Stage 1 — `Reset_Handler` (Assembly startup)

**File:** [src/startup_stm32l496xx.s](../src/startup_stm32l496xx.s)

This is the very first code that executes. It runs entirely in assembly because
the C runtime environment does not exist yet — there is no stack frame, no
initialised globals, nothing.

### 1.1 — Set Stack Pointer explicitly

```asm
ldr sp, =_estack
```

Although the hardware already loaded SP from the vector table, this line
reinitialises it explicitly. This matters when a debugger attaches mid-session
or when the application performs a soft reset, as these paths may bypass the
hardware vector-table load.

`_estack` is the top of SRAM1 (address `0x20040000`) defined in the linker
script:

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20000000 + 256 KB */
```

### 1.2 — Copy `.data` from Flash to RAM

```asm
CopyDataInit:
    ldr r3, =_sidata          ; source: load address in Flash
    ldr r3, [r3, r1]
    str r3, [r0, r1]          ; destination: run address in RAM
    adds r1, r1, #4
```

C global and static variables that have an initial value (e.g. `int x = 5;`)
are **stored in Flash** but must **live in RAM** so they can be modified at
runtime. The linker places the initial values at `_sidata` (in Flash) and
reserves space at `_sdata`–`_edata` (in RAM). This loop copies them across
word by word.

**Why:** Without this, any initialised global would read as zero or garbage.

### 1.3 — Zero `.bss`

```asm
FillZerobss:
    movs r3, #0
    str  r3, [r2], #4
```

Zero-initialised globals and statics (`int y;` or `static int z = 0;`) occupy
the `.bss` section in RAM. The C standard guarantees they start at zero, but
Flash/RAM contain unknown values after power-on, so this loop explicitly zeroes
`_sbss`–`_ebss`.

**Why:** The C standard (§6.7.9) requires static-duration objects with no
explicit initialiser to be zero. Violating this produces unpredictable
behaviour.

### 1.4 — Call `SystemInit()`

```asm
bl SystemInit
```

A pre-`main` hook provided by `system_stm32l4xx.c`. In this project it only
enables the FPU coprocessor (see Stage 2). Full clock configuration is deferred
to `boot_init()` so it can be done from C with proper error handling.

### 1.5 — Call `__libc_init_array()`

```asm
bl __libc_init_array
```

Runs any C++ static constructors and library initialisers registered in the
`.init_array` section. Not strictly needed for pure C projects, but kept for
correctness and future C++ compatibility.

### 1.6 — Branch to `main()`

```asm
bl main
```

Control passes to the C application. If `main()` ever returns (it shouldn't),
`Reset_Handler` falls into an infinite loop.

---

## Stage 2 — `SystemInit()` (FPU enable)

**File:** [src/system_stm32l4xx.c](../src/system_stm32l4xx.c)

The Cortex-M4 has a single-precision FPU that is **disabled by default** at
reset. Any FPU instruction executed while it is disabled triggers a
UsageFault. `SystemInit()` enables it before `main()` runs:

```c
// CPACR register at 0xE000ED88 — set CP10 and CP11 to full access
*((volatile uint32_t *)0xE000ED88U) |= (0xFUL << 20);
__asm volatile("dsb");   // ensure the write completes
__asm volatile("isb");   // flush the pipeline so the new setting takes effect
```

| Bit field | Meaning |
|-----------|---------|
| CP10 (bits 21:20) | Coprocessor 10 = FPU access level |
| CP11 (bits 23:22) | Coprocessor 11 = FPU access level (must match CP10) |
| Value `0b11` | Full access from privileged and unprivileged code |

**Why here and not in `boot_init()`:** `SystemInit()` runs before `.data`/`.bss`
are set up; it must not use any global variables. Enabling the FPU has no such
constraint, so it fits naturally here.

---

## Stage 3 — `boot_init()` (Clock tree)

**File:** [src/boot/boot.c](../src/boot/boot.c)  
**Called from:** `main()` — first line.

At this point the MCU is running on the **MSI oscillator at 4 MHz** (hardware
reset default). `boot_init()` switches it to **80 MHz via the PLL** in a
strictly ordered sequence.

### Why the order matters

The STM32L4 reference manual (RM0351) imposes a dependency chain:

```
Voltage level  →  Flash latency  →  PLL / SYSCLK
```

Increasing the clock speed before raising the voltage or increasing the Flash
wait states causes the CPU to fetch instructions faster than the Flash or the
logic can respond, producing hard-to-diagnose lock-ups or incorrect reads.

### Step-by-step

#### 3.1 — Ensure MSI is stable at 4 MHz

```c
RCC->CR |= RCC_CR_MSION_Msk;
while (!(RCC->CR & RCC_CR_MSIRDY_Msk)) {}   // spin until stable
// Set MSIRANGE = 6 (4 MHz), MSIRGSEL = 1 (use CR range, not RTC)
```

The MSI (Multi-Speed Internal) oscillator is the safe, slow clock used as a
known-good reference while we reconfigure everything else.

**Why:** The PLL requires a stable input. Switching SYSCLK away from the PLL
first ensures there is always a valid clock source during reconfiguration.

#### 3.2 — Raise CPU voltage to Range 1

```c
RCC->APB1ENR1 |= (1UL << 28);   // enable PWR peripheral clock first
PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | (1UL << PWR_CR1_VOS_Pos);
while (PWR->SR2 & PWR_SR2_VOSF_Msk) {}     // wait for regulator to settle
```

The internal voltage regulator has two ranges:

| Range | Max SYSCLK | Typical use |
|-------|-----------|-------------|
| Range 2 (default) | 26 MHz | Low-power, battery-sensitive |
| **Range 1** | **80 MHz** | Full performance |

**Why:** Running at 80 MHz in Range 2 violates the electrical specifications.
The PWR peripheral clock must be enabled before any PWR register is written,
otherwise the write is silently ignored.

#### 3.3 — Set Flash wait states

```c
FLASH_->ACR = (4U << FLASH_ACR_LATENCY_Pos)   // 4 wait states
            | FLASH_ACR_PRFTEN_Msk             // prefetch buffer
            | FLASH_ACR_ICEN_Msk               // instruction cache
            | FLASH_ACR_DCEN_Msk;              // data cache
while ((FLASH_->ACR & FLASH_ACR_LATENCY_Msk) != (4U << ...)) {}
```

Flash memory has a fixed access time (~30 ns on STM32L4). At 80 MHz each CPU
cycle is 12.5 ns, so the CPU would outrun the Flash without wait states. The
required number at 80 MHz / Range 1 / 3.3 V is **4 WS** (RM0351 Table 9).

Enabling the prefetch buffer and caches hides most of the latency cost.

**Why:** Insufficient wait states cause the CPU to read stale or garbage data
from Flash, leading to silent corruption or UsageFault exceptions.

#### 3.4 — Configure and start the PLL

```
MSI 4 MHz  →  ÷ PLLM(1)  →  4 MHz  →  × PLLN(40)  →  160 MHz  →  ÷ PLLR(2)  →  80 MHz
```

```c
RCC->PLLCFGR = (1UL << 0)   // PLLSRC = MSI
             | (0UL << 4)   // PLLM = 0 → ÷1
             | (40UL << 8)  // PLLN = 40
             | (0UL << 25)  // PLLR = 0 → ÷2
             | (1UL << 24); // PLLREN (enable R output)
RCC->CR |= (1UL << 24);                    // PLLON
while (!(RCC->CR & (1UL << 25))) {}        // wait PLLRDY
```

#### 3.5 — Switch SYSCLK to PLL

```c
RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (3UL << RCC_CFGR_SW_Pos);
while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (3UL << RCC_CFGR_SWS_Pos)) {}
```

`SW = 0b11` selects the PLL as SYSCLK. The hardware confirms the switch via
the `SWS` (System Clock Switch Status) bits, which is what we poll.

**Why poll:** The switch is not instantaneous. Writing `SW` and immediately
running at full speed without waiting for `SWS` can cause a brief period where
the CPU runs on no valid clock.

#### 3.6 — Configure SysTick for 1 ms

```c
SysTick->LOAD = 80000U - 1U;   // 80 000 ticks at 80 MHz = exactly 1 ms
SysTick->VAL  = 0U;
SysTick->CTRL = CLKSOURCE | TICKINT | ENABLE;
```

SysTick is a 24-bit down-counter built into every Cortex-M core. It generates
an interrupt (`SysTick_Handler`) every time it reaches zero, which increments
`s_tick_ms`. This is the timebase used by `boot_get_tick()` and `delay_ms()`.

**Why configure it in boot_init():** A reliable millisecond counter must be
ready before any peripheral code runs. If the clock speed were wrong here, all
timing throughout the application would be wrong by the same ratio.

---

## Stage 4 — Peripheral init in `main()`

**File:** [src/main.c](../src/main.c)

After `boot_init()` returns, `main()` initialises application peripherals. At
this point the full C environment is available and the CPU is running at 80 MHz
with a 1 ms tick.

### GPIO — LD1 (PC7)

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN_Msk;   // 1. enable GPIOC bus clock
__asm volatile("dsb");                        // 2. ensure clock is active
GPIOC->MODER = (GPIOC->MODER & ~(3 << 14)) | (1 << 14);  // 3. output mode
```

| Step | Register | Why |
|------|----------|-----|
| Enable clock | `RCC->AHB2ENR` | GPIO peripherals are clock-gated off by default to save power. Writing to any GPIO register while its clock is off has no effect |
| DSB barrier | — | The AHB bus write may not be visible to the AHB2 peripheral bus immediately. A Data Synchronisation Barrier ensures the clock enable has propagated before the GPIO registers are accessed |
| Set MODER | `GPIOC->MODER` | Pins reset to Analog mode (`0b11`). Must be explicitly set to General Purpose Output (`0b01`) |

---

## Memory Map Summary

```
Flash (0x08000000 – 0x080FFFFF)  1 MB
├── .isr_vector   vector table (word 0 = _estack, word 1 = Reset_Handler, …)
├── .text         compiled code (Reset_Handler, boot_init, main, …)
├── .rodata       read-only constants (string literals, const tables)
└── .data (LMA)   initial values for initialised globals — copied to RAM at boot

SRAM1 (0x20000000 – 0x2003FFFF)  256 KB
├── .data (VMA)   initialised globals/statics (populated by Reset_Handler)
├── .bss          zero-initialised globals/statics (zeroed by Reset_Handler)
├── heap          grows upward (unused in this project)
└── stack         grows downward from _estack (0x20040000)

SRAM2 (0x10000000 – 0x1000FFFF)  64 KB  — unused in this project
```

---

## Key Concepts Glossary

| Term | Meaning |
|------|---------|
| **LMA** (Load Memory Address) | Where data is stored in Flash |
| **VMA** (Virtual Memory Address) | Where data lives at runtime (RAM) |
| **MSI** | Multi-Speed Internal oscillator — the default 4 MHz clock at reset |
| **PLL** | Phase-Locked Loop — frequency multiplier; takes 4 MHz MSI → 80 MHz |
| **Wait states (WS)** | CPU stall cycles inserted so Flash can complete a read |
| **Voltage scaling** | Regulator output level; higher voltage allows higher CPU frequency |
| **SysTick** | 24-bit Cortex-M core timer; used here as 1 ms timebase |
| **Vector table** | Array of function pointers at `0x08000000`; first entry is the initial SP, rest are ISR addresses |
| **`.data`** | ELF section for initialised globals — has both an LMA (Flash) and a VMA (RAM) |
| **`.bss`** | ELF section for zero-initialised globals — VMA only, no Flash storage needed |
| **Weak symbol** | Default handler that can be overridden by defining the same name without `__weak__`; used for all IRQ handlers |
