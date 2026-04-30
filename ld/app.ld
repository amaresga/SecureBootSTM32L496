/* app.ld — STM32L496ZGTx application region
 * Flash: 512 KB @ 0x08080000 (Bank 2)
 * SRAM1: 256 KB @ 0x20000000
 * SRAM2:  64 KB @ 0x10000000
 *
 * The bootloader jumps to the reset vector at 0x08080004 after verification.
 * The application's SystemInit() must set SCB->VTOR = 0x08080000.
 */

ENTRY(Reset_Handler)

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* Top of SRAM1 — initial stack pointer */
_sstack = _estack - _Min_Stack_Size;   /* Bottom of reserved stack region — sentinel address */

_Min_Heap_Size  = 0x400;   /* 1 KB  */
_Min_Stack_Size = 0x800;   /* 2 KB  */

/* Application Flash base — must match APP_BASE in bootloader/src/main.c */
APP_FLASH_BASE = 0x08080000;

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08080000, LENGTH = 512K
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 256K
    RAM2  (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}

SECTIONS
{
    /* Image header at APP_BASE; padded to 512 B so .isr_vector is VTOR-aligned. */
    .image_header :
    {
        KEEP(*(.image_header))
        . = ALIGN(512);
    } >FLASH

    /* Vector table at APP_BASE + 512 = 0x08080200 (VTOR = APP_VTOR_BASE). */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM        : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN (__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN (__fini_array_end = .);
    } >FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >RAM

    /DISCARD/ :
    {
        libc.a ( * )
        libm.a ( * )
        libgcc.a ( * )
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
