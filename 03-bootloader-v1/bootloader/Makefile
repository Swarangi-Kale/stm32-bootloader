TARGET = bootloader

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O0 -g \
          -Wall -fdata-sections -ffunction-sections	\
		  -Iinc

LDFLAGS = -T stm32g070.ld -mcpu=cortex-m0plus -mthumb \
          -Wl,--gc-sections -nostdlib	\
		  -Wl,-Map=$(TARGET).map

SRCS    = src/bootloader.c src/startup.c
OBJS    = $(SRCS:.c=.o)

OPENOCD = C:/msys64/mingw64/bin/openocd.exe
OPENOCD_FLAGS = -f interface/stlink.cfg -f target/stm32g0x.cfg

OBJDUMP = arm-none-eabi-objdump

SIZE = arm-none-eabi-size

all: $(TARGET).bin

%.o: %.c
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
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).map $(TARGET).lst