CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := boot_test.elf

ROOT    := ../..
GEN_INC := /tmp/gen

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
    $(ROOT)/arch/tricore/ctx_switch.S \
    $(ROOT)/arch/tricore/vectors.S \
    $(ROOT)/kernel/kernel_main.c \
    $(ROOT)/kernel/printk/ul_printk.c \
    $(ROOT)/kernel/sched/sched.c \
    $(ROOT)/kernel/sched/fifo_rt.c \
    $(ROOT)/kernel/irq/irq.c \
    $(ROOT)/kernel/mem/mem.c \
    $(ROOT)/kernel/thread/thread.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c \
    $(ROOT)/kernel/syscall/syscall_router.c \
    $(ROOT)/kernel/timer/timer.c \
    $(ROOT)/stub/board_init_stub.c

# Test-specific overrides
TEST_SRCS := \
    boot_root_thread.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

# Pass/fail sentinels expected in QEMU stdout (both must appear).
SENTINEL_BSS  := boot_test: bss zero check — OK
SENTINEL_BOOT := BOOT OK

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ul
	@printf '#ifndef UL_CONFIG_H\n#define UL_CONFIG_H\n' > $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_THREADS      16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_ENDPOINTS    16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_NOTIFS       16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_TICK_HZ          1000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ul/config.h
	@printf '#endif\n' >> $(GEN_INC)/ul/config.h

all: gen_config $(TARGET)

$(TARGET): $(SRCS) boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running boot integration test ---"
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | tee /tmp/boot_test.log ; \
	PASS=1; \
	if ! grep -q "$(SENTINEL_BSS)" /tmp/boot_test.log; then \
	    echo "BOOT INTEGRATION TEST: FAIL (sentinel '$(SENTINEL_BSS)' not found)"; \
	    PASS=0; \
	fi; \
	if ! grep -q "$(SENTINEL_BOOT)" /tmp/boot_test.log; then \
	    echo "BOOT INTEGRATION TEST: FAIL (sentinel '$(SENTINEL_BOOT)' not found)"; \
	    PASS=0; \
	fi; \
	if [ "$$PASS" = "1" ]; then \
	    echo "BOOT INTEGRATION TEST: PASS"; \
	else \
	    exit 1; \
	fi

clean:
	rm -f $(TARGET) /tmp/boot_test.log
