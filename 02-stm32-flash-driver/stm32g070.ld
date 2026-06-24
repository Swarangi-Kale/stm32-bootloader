/* Entry Point */
ENTRY(Reset_Handler)

MEMORY{
    FLASH (rx): ORIGIN = 0x08000000, LENGTH = 128K
    SRAM (xrw): ORIGIN = 0x20000000, LENGTH = 36K
}

/* Highest address of the user mode stack */
_estack = ORIGIN(SRAM) + LENGTH(SRAM); 

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .data :
    {
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > FLASH

    _sidata = LOADADDR(.data); 

    .bss :
    {
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss.*)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > SRAM
}