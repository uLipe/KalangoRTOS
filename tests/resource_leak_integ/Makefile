CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := resource_leak_test.elf

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
    $(ROOT)/kernel/mem/phys_alloc.c \
    $(ROOT)/kernel/thread/thread.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c \
    $(ROOT)/kernel/syscall/syscall_router.c \
    $(ROOT)/kernel/timer/timer.c \
    $(ROOT)/stub/board_init_stub.c

TEST_SRCS := \
    resource_leak_test.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

# MAX_THREADS must fit: 1 root + 1 supervisor + WAVES * THREADS_PER_WAVE
# = 1 + 1 + 20 * 8 = 162 → use 170 for margin.
SENTINELS := \
    "resource_leak: start" \
    "resource_leak: wave 10/20 done" \
    "resource_leak: wave 20/20 done" \
    "resource_leak: PASS"

QEMU_TIMEOUT := 120
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ul
	@printf '#ifndef UL_CONFIG_H\n#define UL_CONFIG_H\n' > $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_THREADS      170\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_ENDPOINTS    16\n'  >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_NOTIFS       16\n'  >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_MAX_IRQ_BINDINGS 16\n'  >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_TICK_HZ          1000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_DEBUG_PRINTK     1\n'   >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_US    10000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_TICKS 10\n' >> $(GEN_INC)/ul/config.h
	@printf '#endif\n' >> $(GEN_INC)/ul/config.h

all: gen_config $(TARGET)

$(TARGET): $(SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running resource leak integration test (timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | tee /tmp/resource_leak_test.log ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" /tmp/resource_leak_test.log ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "RESOURCE LEAK TEST: PASS" ; \
	else \
	    echo "RESOURCE LEAK TEST: FAIL" ; \
	    cat /tmp/resource_leak_test.log ; \
	    exit 1 ; \
	fi

clean:
	rm -f $(TARGET) /tmp/resource_leak_test.log
