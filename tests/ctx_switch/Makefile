CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB

# ---------------------------------------------------------------------------
# Simulator selection: EMU=tsim (default when TSIM_PATH is set) or EMU=qemu
# ---------------------------------------------------------------------------
ROOT    := ../..
TSIM_PATH   ?=
TSIM_BIN    := $(TSIM_PATH)/bin/tsim16p_e
TSIM_TC27X  := $(ROOT)/boards/tsim_tc27x
TSIM_MCFG   := $(TSIM_TC27X)/MConfig
TSIM_OCFG   := $(TSIM_TC27X)/OConfig
HAS_TSIM    := $(if $(TSIM_PATH),$(shell test -x "$(TSIM_BIN)" && echo 1 || echo 0),0)
ifeq ($(EMU),)
  ifeq ($(HAS_TSIM),1)
    EMU := tsim
  else
    EMU := qemu
  endif
endif
TARGET  := ctx_switch_test.elf
STRESS  := ctx_stress_test.elf

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

# Per-simulator console source and arch overrides
ifeq ($(EMU),tsim)
CONSOLE_SRC := $(ROOT)/boards/tsim_tc27x/tsim_console.c
CFLAGS      += -DUL_ARCH_IDLE_IS_WAIT=0
else
CONSOLE_SRC := $(ROOT)/boards/qemu_tc27x/qemu_console.c
CFLAGS      += -DUL_ARCH_QEMU_VIRT_CONSOLE=1
CFLAGS      += -DUL_ARCH_STM0_BASE=0xF0000000u
CFLAGS      += -DUL_ARCH_SRC_STM0_SR0=0xF0038300u
CFLAGS      += -DUL_ARCH_SRC_SRE_BIT=10u
CFLAGS      += -DUL_ARCH_RAM_BASE=0x90000000u
CFLAGS      += -DUL_ARCH_IDLE_IS_WAIT=0
endif

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
    $(CONSOLE_SRC)

STRESS_TEST_SRCS := \
    ctx_switch_stress.c \
    $(CONSOLE_SRC)

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
	@printf '#define UL_CONFIG_SCHED_QUANTUM_US    10000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_TICKS 10\n' >> $(GEN_INC)/ul/config.h
	@printf '#endif\n' >> $(GEN_INC)/ul/config.h

all: gen_config $(TARGET) $(STRESS)

$(TARGET): $(KERNEL_SRCS) $(BASIC_TEST_SRCS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

$(STRESS): $(KERNEL_SRCS) $(STRESS_TEST_SRCS)
	$(CC) $(CFLAGS) $^ $(STRESS_LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

ifeq ($(EMU),tsim)
run: run_tsim
else
run: run_qemu
endif

run_tsim: $(TARGET)
	@echo "--- running ctx switch (TSIM) ---"
	@timeout 180 $(TSIM_BIN) \
	    -MConfig $(TSIM_MCFG) \
	    -OConfig $(TSIM_OCFG) \
	    -s -H -disable-watchdog -x 0 \
	    -o $(TARGET) </dev/null 2>&1 | tee /tmp/ctx_switch_test.log ; \
	PASS=1; \
	if [ "$$PASS" = "1" ]; then echo "CTX SWITCH: PASS"; else exit 1; fi

run_qemu: $(TARGET)
	@echo "--- running ctx_switch integration test ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee /tmp/ctx_switch_test.log ; \
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
	    </dev/null 2>&1 | tee /tmp/ctx_stress_test.log ; \
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
