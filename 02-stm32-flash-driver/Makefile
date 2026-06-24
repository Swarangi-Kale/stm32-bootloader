TARGET = main

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O0 -g \
          -Wall -fdata-sections -ffunction-sections \
		  -Iinc

LDFLAGS = -T stm32g070.ld -mcpu=cortex-m0plus -mthumb \
          -Wl,--gc-sections -nostdlib

DEPS = inc/flash.h

SRCS    = src/main.c src/startup.c src/flash.c
OBJS    = $(SRCS:.c=.o)

OPENOCD = C:/msys64/mingw64/bin/openocd.exe
OPENOCD_FLAGS = -f interface/stlink.cfg -f target/stm32g0x.cfg

all: $(TARGET).bin

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

flash: $(TARGET).elf
	$(OPENOCD) $(OPENOCD_FLAGS) -c "program $(TARGET).elf verify reset exit"

debug:
	$(OPENOCD) $(OPENOCD_FLAGS) -c "init" -c "reset halt"

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin


/* use command to run this makefile: "make -f Makefile.blinky" */