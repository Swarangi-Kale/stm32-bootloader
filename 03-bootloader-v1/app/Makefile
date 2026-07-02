TARGET = app

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O0 -g \
          -Wall -fdata-sections -ffunction-sections	\
		  -Iinc

LDFLAGS = -T stm32g070.ld -mcpu=cortex-m0plus -mthumb \
          -Wl,--gc-sections -nostdlib	\
		  -Wl,-Map=$(TARGET).map

DEPS = inc/blinky.h

SRCS    = src/main.c src/blinky.c src/startup.c
ASMS    = src/bootloader.s
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
