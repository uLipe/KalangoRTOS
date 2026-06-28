CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := ctx_switch_test.elf
STRESS  := ctx_stress_test.elf

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

# SVLCX/RSLCX/RFE implementation: RSLCX and RFE free CSA frames via hardware
# instructions — no mtcr FCX required and no per-switch leak in QEMU.
# The stress test intentionally uses the same 64-frame default pool so that
# any regression (leak) causes FCU trap → hang → timeout → FAIL.
STRESS_LDFLAGS := $(LDFLAGS)

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
    $(ROOT)/kernel/timer/timer.c \
    $(ROOT)/kernel/mem/phys_alloc.c \
    $(ROOT)/stub/board_init_stub.c

BASIC_TEST_SRCS := \
    ctx_switch_root_thread.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

STRESS_TEST_SRCS := \
    ctx_switch_stress.c \
    $(ROOT)/boards/qemu_tc27x/qemu_console.c

SENTINELS       := "switching to root thread" "ROOT THREAD RUNNING"
STRESS_SENTINELS := "ctx_stress: start" "ctx_stress: PASS"

# Timeout before QEMU is killed.  --kill-after sends SIGKILL if SIGTERM is
# ignored (QEMU AURIX TCG loop can block signal delivery).
QEMU_TIMEOUT := 120
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run stress clean gen_config

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

all: gen_config $(TARGET) $(STRESS)

$(TARGET): $(KERNEL_SRCS) $(BASIC_TEST_SRCS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

$(STRESS): $(KERNEL_SRCS) $(STRESS_TEST_SRCS)
	$(CC) $(CFLAGS) $^ $(STRESS_LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running ctx_switch integration test ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>/dev/null | tee /tmp/ctx_switch_test.log ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" /tmp/ctx_switch_test.log ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "CTX_SWITCH INTEGRATION TEST: PASS" ; \
	else \
	    echo "CTX_SWITCH INTEGRATION TEST: FAIL" ; \
	    exit 1 ; \
	fi

stress: $(STRESS)
	@echo "--- running ctx_switch stress test (timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(STRESS) \
	    -nographic \
	    </dev/null 2>/dev/null | tee /tmp/ctx_stress_test.log ; \
	FAIL=0 ; \
	for S in $(STRESS_SENTINELS) ; do \
	    if grep -q "$$S" /tmp/ctx_stress_test.log ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "CTX_STRESS TEST: PASS" ; \
	else \
	    echo "CTX_STRESS TEST: FAIL" ; \
	    exit 1 ; \
	fi

clean:
	rm -f $(TARGET) $(STRESS) \
	    /tmp/ctx_switch_test.log /tmp/ctx_stress_test.log
