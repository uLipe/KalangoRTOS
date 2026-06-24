CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := boot_test.elf

ROOT    := ../..
GEN_INC := /tmp/gen   # directory containing generated ul/config.h

CFLAGS := \
    -mcpu=tc27xx \
    -ffreestanding \
    -ffunction-sections \
    -fdata-sections \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(ROOT)/include \
    -I$(ROOT)/arch/tricore/include \
    -I$(ROOT) \
    -I$(GEN_INC) \
    -O0 -g

LDFLAGS := \
    -T boot_test.ld \
    -mcpu=tc27xx \
    -nostartfiles \
    -Wl,--gc-sections \
    -Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc

# Real kernel boot path sources
KERNEL_SRCS := \
    $(ROOT)/arch/tricore/startup.S \
    $(ROOT)/arch/tricore/arch.c \
    $(ROOT)/arch/tricore/vectors.S \
    $(ROOT)/kernel/kernel_main.c \
    $(ROOT)/kernel/printk/ul_printk.c \
    $(ROOT)/kernel/sched/sched.c \
    $(ROOT)/kernel/irq/irq.c \
    $(ROOT)/kernel/mem/mem.c \
    $(ROOT)/kernel/thread/thread.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c \
    $(ROOT)/kernel/syscall/syscall_router.c \
    $(ROOT)/stub/board_init_stub.c

# Test-specific overrides
TEST_SRCS := \
    boot_root_thread.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

# Pass/fail sentinel expected in QEMU stdout.
BOOT_SENTINEL := BOOT OK

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running boot integration test ---"
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    2>/dev/null | tee /tmp/boot_test.log ; \
	if grep -q "$(BOOT_SENTINEL)" /tmp/boot_test.log; then \
	    echo "BOOT INTEGRATION TEST: PASS"; \
	else \
	    echo "BOOT INTEGRATION TEST: FAIL (sentinel '$(BOOT_SENTINEL)' not found)"; \
	    exit 1; \
	fi

clean:
	rm -f $(TARGET) /tmp/boot_test.log
