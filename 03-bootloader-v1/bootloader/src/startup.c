    #include <stdint.h>

    // symbols from linker script
    extern uint32_t _sidata;
    extern uint32_t _sdata;
    extern uint32_t _edata;
    extern uint32_t _sbss;
    extern uint32_t _ebss;
    extern uint32_t _estack;

    // forward declare main
    int main(void);

    void Reset_Handler(void);
    void Default_Handler(void);

    // weak aliases — any unimplemented IRQ falls through to Default_Handler
    void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
    void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));

    // vector table — must be first thing in Flash
    __attribute__((section(".isr_vector")))
    uint32_t vectors[] = {
        (uint32_t)&_estack,          // initial stack pointer
        (uint32_t)&Reset_Handler,    // reset
        (uint32_t)&NMI_Handler,
        (uint32_t)&HardFault_Handler,
        // add more IRQ slots here as needed
    };

    void Default_Handler(void) { while(1); }
    void Reset_Handler(void) {
        
        // copy .data from Flash to RAM
        uint32_t *src = &_sidata;
        uint32_t *dst = &_sdata;
        while (dst < &_edata) *dst++ = *src++;

        // init. the .bss section to zero in SRAM
        dst = &_sbss;
        while (dst < &_ebss) *dst++ = 0;

        // invoke main
        main();
        
        while(1); // should never reach here
    }