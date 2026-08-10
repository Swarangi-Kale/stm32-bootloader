TARGET = main

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O0 -g \
          -Wall -fdata-sections -ffunction-sections	\
		  -Iinc\
		  -I../Common_Drivers/inc	\
		  -I../shared/inc

LDFLAGS = -T stm32g070.ld -mcpu=cortex-m0plus -mthumb \
          -Wl,--gc-sections -nostartfiles	\
		  -Wl,-Map=$(TARGET).map

DEPS = inc/comms.h
DEPS += ../Common_Drivers/inc/uart.h
DEPS += ../Common_Drivers/inc/ringbuffer.h
DEPS += ../Common_Drivers/inc/chip_config_init.h
DEPS += ../Common_Drivers/inc/crc.h
DEPS += ../Common_Drivers/inc/blinky.h
DEPS += ../Common_Drivers/inc/systick.h
DEPS += ../Common_Drivers/inc/simple-timer.h
DEPS += ../Common_Drivers/inc/flash.h
DEPS += ../shared/inc/firmware_info.h
DEPS += ../shared/inc/boot_status.h

SRCS = src/startup.c src/comms.c
SRCS += src/bootloader.c
SRCS += ../Common_Drivers/src/chip_config_init.c
SRCS += ../Common_Drivers/src/ringbuffer.c
SRCS += ../Common_Drivers/src/uart.c
SRCS += ../Common_Drivers/src/crc.c
SRCS += ../Common_Drivers/src/blinky.c
SRCS += ../Common_Drivers/src/systick.c
SRCS += ../Common_Drivers/src/simple-timer.c
SRCS += ../Common_Drivers/src/flash.c

OBJS    = $(SRCS:.c=.o) $(ASMS:.s=.o)

OPENOCD = C:/msys64/mingw64/bin/openocd.exe
OPENOCD_FLAGS = -f interface/stlink.cfg -f target/stm32g0x.cfg

OBJDUMP = arm-none-eabi-objdump

SIZE = arm-none-eabi-size

all: $(TARGET).bin

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	python pad-bootloader.py

$(TARGET).lst: $(TARGET).elf
	$(OBJDUMP) -D $< > $@

flash: $(TARGET).elf
	$(OPENOCD) $(OPENOCD_FLAGS) -c "program $(TARGET).elf verify reset exit"

debug:
	$(OPENOCD) $(OPENOCD_FLAGS) -c "init" -c "reset halt"

size: $(TARGET).elf
	$(SIZE) $(TARGET).elf

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map
