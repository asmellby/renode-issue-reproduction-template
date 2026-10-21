CROSS ?= arm-none-eabi-
CC = $(CROSS)gcc
CFLAGS = -mcpu=cortex-m33 -mthumb -mfloat-abi=soft -O1 -g -ffreestanding -nostdlib -Wall -Werror -Icommon
LDFLAGS = -T common/link.ld -nostdlib -Wl,--build-id=none

test.elf: test.c common/startup.c common/common.h common/link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ test.c common/startup.c -lgcc

clean:
	rm -f test.elf
