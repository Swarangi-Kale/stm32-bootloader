#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

#define BOOTLOADER_SIZE (0x8000U) 

//flash layout
#define FLASH_BASE_ADDR (0x08000000U)
#define APP_HEADER_START (0x08008000U) //Size = 2kB (one page)
#define APP_START_ADDR (0x08008800U)
#define FLASH_MEM_END (0x08020000U) //128kB flash

//SRAM layout
#define SRAM_START_ADDR (0x20000000U)
#define SRAM_END_ADDR (0x20009000U) //36kB SRAM

#endif /* MEM_LAYOUT_H */