TARGET = main

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O0 -g \
          -Wall -fdata-sections -ffunction-sections	\
		  -Imcu-firmware/inc \
		  -ICommon_Drivers/inc

LDFLAGS = -T stm32g070.ld -mcpu=cortex-m0plus -mthumb \
          -Wl,--gc-sections -nostartfiles	\
		  -Wl,-Map=$(TARGET).map

DEPS = mcu-firmware/inc/comms.h Common_Drivers/inc/uart.h Common_Drivers/inc/ringbuffer.h Common_Drivers/inc/chip_config_init.h Common_Drivers/inc/crc.h Common_Drivers/inc/blinky.h

SRCS    = mcu-firmware/src/startup.c mcu-firmware/src/comms.c mcu-firmware/src/packet_protocol.c Common_Drivers/src/chip_config_init.c Common_Drivers/src/ringbuffer.c Common_Drivers/src/uart.c Common_Drivers/src/crc.c Common_Drivers/src/blinky.c

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
