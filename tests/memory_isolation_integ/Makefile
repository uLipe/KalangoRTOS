CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
ROOT    := ../..
TARGET  := memory_isolation_test.elf
GEN_INC := /tmp/gen

CONSOLE_SRC := $(ROOT)/boards/qemu_tc3xx/qemu_console.c

CFLAGS := \
    -mcpu=tc39xx \
    -ffreestanding \
    -ffunction-sections \
    -fdata-sections \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -DULMK_KERNEL_BUILD \
    -I$(ROOT)/include \
    -I$(ROOT)/arch/tricore/include \
    -I$(ROOT) \
    -I$(GEN_INC) \
    -O0 -g \
    -DULMK_ARCH_QEMU_VIRT_CONSOLE=1 \
    -DULMK_ARCH_SRC_STM0_SR0=0xF0038300u \
    -DULMK_ARCH_SRC_SRE_BIT=10u \
    -DULMK_ARCH_IDLE_IS_WAIT=0

LDFLAGS := \
    -T ../boot/boot_test.ld \
    -mcpu=tc39xx \
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
    $(ROOT)/kernel/printk/ulmk_printk.c \
    $(ROOT)/kernel/sched/sched.c \
    $(ROOT)/kernel/sched/fifo_rt.c \
    $(ROOT)/kernel/sched/bitmap_rt.c \
    $(ROOT)/kernel/irq/irq.c \
    $(ROOT)/kernel/mem/mem.c \
    $(ROOT)/kernel/mem/tlsf.c \
    $(ROOT)/kernel/thread/thread.c \
    $(ROOT)/kernel/ipc/ep.c \
    $(ROOT)/kernel/notif/notif.c \
    $(ROOT)/kernel/syscall/syscall_router.c \
    $(ROOT)/stub/board_init_stub.c

TEST_SRCS := \
    memory_isolation_test.c \
    $(CONSOLE_SRC) \
    $(ROOT)/boards/qemu_tc3xx/board_timer.c

SRCS := $(KERNEL_SRCS) $(TEST_SRCS)

SENTINELS := \
    "mem_iso: start" \
    "mem_iso: scenario 1 (anon mmap) PASS" \
    "mem_iso: scenario 2 (grant read) PASS" \
    "mem_iso: scenario 3 (MPU fault) P" \
    "MEMORY ISOLATION TEST: PASS"

QEMU_TIMEOUT := 90
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ulmk
	@printf '#ifndef ULMK_CONFIG_H\n#define ULMK_CONFIG_H\n' > $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#endif\n' >> $(GEN_INC)/ulmk/config.h

all: gen_config $(TARGET)

$(TARGET): $(SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running memory isolation test (timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee /tmp/memory_isolation_test.log ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" /tmp/memory_isolation_test.log ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "MEMORY ISOLATION TEST: PASS" ; \
	else \
	    echo "MEMORY ISOLATION TEST: FAIL" ; \
	    cat /tmp/memory_isolation_test.log ; \
	    exit 1 ; \
	fi

clean:
	rm -f $(TARGET) /tmp/memory_isolation_test.log
