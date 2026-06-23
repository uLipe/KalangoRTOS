CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := hello.elf

CFLAGS  := -mcpu=tc27xx \
           -ffunction-sections \
           -fdata-sections \
           -ffreestanding \
           -fno-exceptions \
           -Wall -Wextra \
           -O0 -g

LDFLAGS := -T linker.ld \
           -mcpu=tc27xx \
           -nostartfiles \
           -Wl,--gc-sections

LIBS    := -lc -lgcc

SRCS    := startup.S platform.c main.c

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors

debug: $(TARGET)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors \
	    -gdb tcp::1234 -S &
	tricore-elf-gdb $(TARGET) \
	    -ex "target remote :1234" \
	    -ex "break main" \
	    -ex "continue"

clean:
	rm -f $(TARGET)

.PHONY: all run debug clean
