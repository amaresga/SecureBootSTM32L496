# STM32L496 Boot Process

This document describes every step from power-on to the first line of `main()`,
and what each stage initialises and why.

---

## Overview

```
Power-on / Reset
      │
      ▼
┌─────────────────────────────────┐
│  Stage 0 — Hardware Reset       │  (Cortex-M4 core, done automatically)
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│  Stage 1 — Reset_Handler        │  startup_stm32l496xx.s
│  • Set stack pointer            │
│  • SRAM1 security scrub (256KB) │
│  • Write stack sentinel         │
│  • Copy .data Flash → RAM       │
│  • Zero .bss                    │
│  • Call SystemInit()            │
│  • Call __libc_init_array()     │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│  Stage 2 — SystemInit()         │  system_stm32l4xx.c
│  • Lock VTOR to Flash           │
│  • Enable fault exceptions      │
│  • Enable divide-by-zero trap   │
│  • Enable FPU                   │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│  Stage 3 — main() → boot_init() │  src/boot/boot.c
│  • Arm IWDG (~2 s timeout)      │
│  • Validate stack sentinel      │
│  • MSI → 4 MHz (stable ref)     │
│  • Voltage Range 1              │
│  • Flash latency 4 WS           │
│  • PLL → 80 MHz SYSCLK          │
│  • SysTick 1 ms                 │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│  Stage 4 — Peripheral init      │  src/main.c
│  • led_init() → PC7 output      │
│  • Blink loop + watchdog kick   │
└─────────────────────────────────┘
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

`_estack` and `_sstack` are defined in the linker script:

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20040000 — top of SRAM1 */
_sstack = _estack - _Min_Stack_Size;   /* 0x2003F800 — bottom of reserved stack (2 KB) */
```

### 1.2 — SRAM1 security scrub

```asm
ldr   r0, =0x20000000
ldr   r1, =_estack
movs  r2, #0
ScrubSRAM:
    str   r2, [r0], #4
    cmp   r0, r1
    bcc   ScrubSRAM
```

All 256 KB of SRAM1 is zeroed before `.data` or `.bss` are initialised. This
prevents residual data from a previous execution — including cryptographic keys,
passwords, or decrypted firmware — from leaking into uninitialised variables,
heap, or stack memory.

**Why here:** This is the only point in the boot sequence where no C data has
been placed in RAM yet, so it is safe to zero everything unconditionally.

### 1.3 — Write stack sentinel

```asm
ldr   r0, =_sstack
ldr   r1, =0xDEADC0DE
str   r1, [r0]
```

The word `0xDEADC0DE` is written at `_sstack` (the bottom of the reserved
2 KB stack region). `boot_init()` validates this value as its second step. A
missing or corrupted sentinel means either the stack grew past the reserved
region before `boot_init()` ran, or the startup code was bypassed entirely.

**Why written after the scrub:** The scrub zeros the entire SRAM including this
address, so the sentinel must be written after the scrub loop, not before.

### 1.4 — Copy `.data` from Flash to RAM

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

**Why:** Without this, any initialised global would read as zero (from the
scrub) regardless of its declared initial value.

### 1.5 — Zero `.bss`

```asm
FillZerobss:
    movs r3, #0
    str  r3, [r2], #4
```

Zero-initialised globals and statics (`int y;` or `static int z = 0;`) occupy
the `.bss` section in RAM. The SRAM scrub already zeroed this region, but this
loop is kept for strict C standard compliance — the C standard (§6.7.9)
guarantees static-duration objects are zero-initialised regardless of hardware
state.

### 1.6 — Call `SystemInit()`

```asm
bl SystemInit
```

Configures SCB security settings and enables the FPU (see Stage 2).

### 1.7 — Call `__libc_init_array()`

```asm
bl __libc_init_array
```

Runs any C++ static constructors and library initialisers registered in the
`.init_array` section. Kept for correctness and future C++ compatibility.

### 1.8 — Branch to `main()`

```asm
bl main
```

Control passes to the C application. If `main()` ever returns (it shouldn't),
`Reset_Handler` falls into an infinite loop.

---

## Stage 2 — `SystemInit()` (Security & FPU)

**File:** [src/system_stm32l4xx.c](../src/system_stm32l4xx.c)

Called by `Reset_Handler` before `.data`/`.bss` are guaranteed to be fully
ready for C code. Must not use any global variables.

### 2.1 — Lock VTOR to Flash

```c
SCB->VTOR = 0x08000000UL;
```

The Vector Table Offset Register controls where the CPU looks for interrupt
handlers. Setting it explicitly to the start of Flash prevents an attacker or
corrupted code from redirecting the vector table to a RAM region they control.

### 2.2 — Enable configurable fault exceptions

```c
SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
            | SCB_SHCSR_BUSFAULTENA_Msk
            | SCB_SHCSR_MEMFAULTENA_Msk;
```

By default all three configurable faults escalate silently to HardFault,
making the root cause hard to diagnose. Enabling them routes each fault to its
dedicated handler (`UsageFault_Handler`, `BusFault_Handler`,
`MemManage_Handler`) which capture a full register snapshot (see `fault.c`).

### 2.3 — Enable divide-by-zero trap

```c
SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
```

Without this, integer division by zero on Cortex-M4 silently returns zero. With
the trap enabled it generates a UsageFault, making the bug visible immediately.

### 2.4 — Enable the FPU

```c
SCB->CPACR |= (0xFUL << 20U);
__asm volatile("dsb");
__asm volatile("isb");
```

Enables CP10 and CP11 (the FPU coprocessors) to full access from both
privileged and unprivileged code. Any FPU instruction executed while the FPU is
disabled triggers a UsageFault.

| Bit field | Meaning |
|-----------|---------|
| CP10 (bits 21:20) | Coprocessor 10 = FPU access level |
| CP11 (bits 23:22) | Coprocessor 11 = must match CP10 |
| Value `0b11` | Full access |

---

## Stage 3 — `boot_init()` (Watchdog, sentinel, clock tree)

**File:** [src/boot/boot.c](../src/boot/boot.c)  
**Called from:** `main()` — first line.

### Why the order matters

```
IWDG arm  →  Sentinel check  →  Voltage level  →  Flash latency  →  PLL / SYSCLK
```

The watchdog is armed first so any subsequent hang — including inside the clock
switch — triggers a hardware reset automatically. The sentinel is checked before
any clock change so a corrupt boot environment is caught before the system
reaches an unpredictable state.

### Step 3.1 — Arm the Independent Watchdog

```c
IWDG->KR  = IWDG_KR_START;   // 0xCCCC — enables LSI oscillator and IWDG
IWDG->KR  = IWDG_KR_UNLOCK;  // 0x5555 — unlock PR and RLR for write
IWDG->PR  = 4U;               // prescaler ÷64
IWDG->RLR = 999U;             // (64 × 1000) / 32 kHz ≈ 2.000 s timeout
while (IWDG->SR & (IWDG_SR_PVU_Msk | IWDG_SR_RVU_Msk)) {}
IWDG->KR  = IWDG_KR_RELOAD;  // 0xAAAA — load new value before first timeout
```

The IWDG runs on the independent LSI (~32 kHz) oscillator and cannot be stopped
once started in normal run mode. Writing `START` (0xCCCC) first is mandatory —
it gates the LSI on, without which `SR` never clears and the register-update
poll would spin forever.

The application must call `boot_kick_watchdog()` at least once every ~2 s or
the device will reset.

### Step 3.2 — Validate stack sentinel

```c
extern uint32_t _sstack[];
return (*(volatile uint32_t *)_sstack == 0xDEADC0DEUL) ? 0 : -1;
```

Reads the word at `_sstack` and compares it to `0xDEADC0DE`. If it does not
match, `boot_init()` returns `BOOT_ERR_STACK_CORRUPT` (-1) and `main()` should
treat this as a fatal error.

**Important:** `_sstack` is a linker-defined absolute symbol. It must be
declared as `extern uint32_t _sstack[]` (array, not scalar) in C so the
compiler treats the symbol's value as an address rather than a variable
location. Using `extern uint32_t _sstack; &_sstack` would read a
compiler-allocated address, not the sentinel word.

### Step 3.3 — Stabilise MSI at 4 MHz

```c
RCC->CR |= RCC_CR_MSION_Msk;
while (!(RCC->CR & RCC_CR_MSIRDY_Msk)) {}
// MSIRANGE = 6 (4 MHz), MSIRGSEL = 1 (use CR range, not RTC backup domain)
RCC->CFGR &= ~RCC_CFGR_SW_Msk;   // SW = 0b00 → MSI as SYSCLK
```

Switches SYSCLK to MSI before touching the PLL, ensuring there is always a
valid clock source during reconfiguration.

### Step 3.4 — Raise CPU voltage to Range 1

```c
RCC->APB1ENR1 |= (1UL << 28);   // enable PWR peripheral clock
__asm volatile("dsb");
PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | (1UL << PWR_CR1_VOS_Pos);
while (PWR->SR2 & PWR_SR2_VOSF_Msk) {}
```

| Range | Max SYSCLK | Default |
|-------|-----------|---------|
| Range 2 | 26 MHz | ✓ (at reset) |
| **Range 1** | **80 MHz** | — |

The PWR peripheral clock must be enabled before writing any PWR register or the
write is silently discarded.

### Step 3.5 — Set Flash wait states

```c
FLASH_->ACR = (4U << FLASH_ACR_LATENCY_Pos)
            | FLASH_ACR_PRFTEN_Msk    // prefetch buffer
            | FLASH_ACR_ICEN_Msk      // instruction cache
            | FLASH_ACR_DCEN_Msk;     // data cache
```

At 80 MHz each CPU cycle is 12.5 ns; Flash access takes ~30 ns. **4 wait
states** are required at 80 MHz / Range 1 / 3.3 V (RM0351 Table 9). The caches
hide most of the latency cost in practice.

### Step 3.6 — Configure and start the PLL

```
MSI 4 MHz → ÷PLLM(1) → 4 MHz → ×PLLN(40) → 160 MHz → ÷PLLR(2) → 80 MHz
```

```c
RCC->PLLCFGR = (1UL << 0)    // PLLSRC = MSI
             | (0UL << 4)    // PLLM = 0 → ÷1
             | (40UL << 8)   // PLLN = 40
             | (0UL << 25)   // PLLR = 0 → ÷2
             | (1UL << 24);  // PLLREN (enable R output)
```

### Step 3.7 — Switch SYSCLK to PLL

```c
RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (3UL << RCC_CFGR_SW_Pos);
while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (3UL << RCC_CFGR_SWS_Pos)) {}
```

`SW = 0b11` selects PLL as SYSCLK. Polling `SWS` ensures the switch is
complete before the CPU runs at full speed — proceeding without waiting risks a
brief window with no valid clock.

### Step 3.8 — Configure SysTick for 1 ms

```c
SysTick->LOAD = 80000U - 1U;   // 80 000 ticks at 80 MHz = exactly 1 ms
SysTick->VAL  = 0U;
SysTick->CTRL = CLKSOURCE | TICKINT | ENABLE;
```

`SysTick_Handler` increments `s_tick_ms` on every reload. `boot_get_tick()`
returns this counter; `delay_ms()` and application timing are built on it.

---

## Fault Handling

**File:** [src/boot/fault.c](../src/boot/fault.c)

All four configurable fault exceptions plus NMI have dedicated naked handlers.
Each uses `EXC_RETURN` in LR to select MSP or PSP, then branches to
`fault_record_and_reset()` which captures the full hardware exception frame and
SCB status registers into `g_fault_record`:

| Field | Source |
|-------|--------|
| `r0`–`r3`, `r12`, `lr`, `pc`, `xpsr` | Hardware exception stack frame |
| `cfsr` | `SCB->CFSR` — MMFSR + BFSR + UFSR combined |
| `hfsr` | `SCB->HFSR` — HardFault status |
| `mmfar` | `SCB->MMFAR` — faulting memory address (MemManage) |
| `bfar` | `SCB->BFAR` — faulting bus address (BusFault) |

**Release builds (`NDEBUG`):** issues `SCB->AIRCR = VECTKEY | SYSRESETREQ`
immediately — the device resets and recovers automatically.  
**Debug builds:** halts in an infinite loop — connect GDB and inspect
`g_fault_record.pc` and `g_fault_record.cfsr` to find the root cause.

NMI specifically handles Clock Security System (CSS) failure: if the HSE
oscillator fails, CSS fires NMI and the handler performs an immediate reset so
the device reinitialises on MSI.

---

## Stage 4 — Peripheral init in `main()`

**File:** [src/main.c](../src/main.c)

After `boot_init()` returns, `main()` initialises application peripherals. The
CPU is running at 80 MHz with a 1 ms SysTick tick and the IWDG armed.

### GPIO — LD1 (PC7)

```c
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN_Msk;   // 1. enable GPIOC bus clock
__asm volatile("dsb");                        // 2. ensure clock is active
GPIOC->MODER = (GPIOC->MODER & ~(3 << 14)) | (1 << 14);  // 3. output mode
```

| Step | Register | Why |
|------|----------|-----|
| Enable clock | `RCC->AHB2ENR` | GPIO peripherals are clock-gated off by default |
| DSB barrier | — | Ensures the AHB2 bus clock-enable has propagated before GPIO registers are accessed |
| Set MODER | `GPIOC->MODER` | Pins reset to Analog mode (`0b11`); must be set to GP Output (`0b01`) |

### Main loop

```c
for (;;) {
  led_on();
  delay_ms(BLINK_HALF);    // 1000 ms
  boot_kick_watchdog();    // IWDG->KR = 0xAAAA
  led_off();
  delay_ms(BLINK_HALF);
  boot_kick_watchdog();
}
```

`boot_kick_watchdog()` is called twice per cycle (once per 1 s half-period) to
ensure the IWDG is refreshed well within its ~2 s window even if one half of
the blink loop were to stall.

---

## Memory Map Summary

```
Flash (0x08000000 – 0x080FFFFF)  1 MB
├── .isr_vector   vector table (word 0 = _estack, word 1 = Reset_Handler, …)
├── .text         compiled code (Reset_Handler, SystemInit, boot_init, main, …)
├── .rodata       read-only constants
└── .data (LMA)   initial values for initialised globals — copied to RAM at boot

SRAM1 (0x20000000 – 0x2003FFFF)  256 KB
├── .data (VMA)   initialised globals/statics  (0x20000000 upward)
├── .bss          zero-initialised globals/statics
├── heap          grows upward (unused in this project)
├── stack         grows downward from _estack (0x20040000)
└── sentinel      1 word at _sstack (0x2003F800) = 0xDEADC0DE

SRAM2 (0x10000000 – 0x1000FFFF)  64 KB  — unused (candidate for .noinit fault log)
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
| **Vector table** | Array at `0x08000000`; word 0 = initial SP, words 1+ = ISR addresses |
| **VTOR** | Vector Table Offset Register — set to `0x08000000` to prevent redirection |
| **IWDG** | Independent Watchdog — LSI-clocked timer that resets the device if not kicked within the timeout |
| **Sentinel** | Magic word (`0xDEADC0DE`) at `_sstack` to detect pre-boot stack overflow or bypassed startup |
| **SRAM scrub** | Zeroing all SRAM before use to prevent data leakage from a previous execution |
| **SCB** | System Control Block — Cortex-M core register block controlling faults, caches, VTOR |
| **CFSR** | Configurable Fault Status Register — combined MMFSR + BFSR + UFSR; primary post-mortem diagnostic |
| **`.data`** | ELF section for initialised globals — has both an LMA (Flash) and a VMA (RAM) |
| **`.bss`** | ELF section for zero-initialised globals — VMA only, no Flash storage needed |
| **Weak symbol** | Default handler that can be overridden by defining the same name without `__weak__` |

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
