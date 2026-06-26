CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := csa_ctx_test.elf

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
    -T ../boot/boot_test.ld \
    -mcpu=tc27xx \
    -nostartfiles \
    -Wl,--gc-sections \
    -Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc

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
    $(ROOT)/stub/board_init_stub.c

TEST_SRCS := \
    csa_ctx_test.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

SENTINEL     := csa_ctx_test: PASS
QEMU_TIMEOUT := 60
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

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

$(TARGET): $(SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running CSA context fabrication test (timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | tee /tmp/csa_ctx_test.log | \
	    grep -m 1 "$(SENTINEL)" ; \
	if grep -q "$(SENTINEL)" /tmp/csa_ctx_test.log; then \
	    echo "CSA CTX TEST: PASS"; \
	else \
	    echo "CSA CTX TEST: FAIL (sentinel '$(SENTINEL)' not found)"; \
	    exit 1; \
	fi

clean:
	rm -f $(TARGET) /tmp/csa_ctx_test.log
