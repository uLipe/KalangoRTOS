CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := test.elf

# Platform files are shared with tools/hello — no duplication.
PLAT_DIR  := ../tools/hello
UNITY_DIR := /opt/unity

CFLAGS := -mcpu=tc27xx \
          -ffunction-sections \
          -fdata-sections \
          -ffreestanding \
          -fno-exceptions \
          -Wall -Wextra \
          -I$(UNITY_DIR) \
          -O0 -g

LDFLAGS := -T $(PLAT_DIR)/linker.ld \
           -mcpu=tc27xx \
           -nostartfiles \
           -Wl,--gc-sections

LIBS := -lc -lgcc

PLAT_SRCS := $(PLAT_DIR)/startup.S \
             $(PLAT_DIR)/platform.c

UNITY_SRC := $(UNITY_DIR)/unity.c

# All test suite files.  Add new suites here as they are implemented.
TEST_SRCS := runner.c \
             test_dummy.c \
             test_linker.c \
             test_thread_api.c \
             test_ipc_api.c \
             test_notif_api.c \
             test_mem_api.c \
             test_irq_api.c \
             test_libul.c

SRCS := $(PLAT_SRCS) $(UNITY_SRC) $(TEST_SRCS)

all: $(TARGET)

$(TARGET): $(SRCS) $(PLAT_DIR)/linker.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

# Interactive run — press Ctrl-A X to exit QEMU.
run: $(TARGET)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors

# CI run — QEMU exits when the test binary calls _exit(); propagate the code.
ci: $(TARGET)
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors ; \
	EXIT=$$? ; \
	echo "--- QEMU exited: $$EXIT ---" ; \
	exit $$EXIT

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

.PHONY: all run ci debug clean
