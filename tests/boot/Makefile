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
TARGET  := boot_test.elf

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
    $(ROOT)/kernel/mem/phys_alloc.c \
    $(ROOT)/stub/board_init_stub.c

# Test-specific overrides
TEST_SRCS := \
    boot_root_thread.c \
    $(CONSOLE_SRC)

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
	@printf '#define UL_CONFIG_SCHED_QUANTUM_US    10000\n' >> $(GEN_INC)/ul/config.h
	@printf '#define UL_CONFIG_SCHED_QUANTUM_TICKS 10\n' >> $(GEN_INC)/ul/config.h
	@printf '#endif\n' >> $(GEN_INC)/ul/config.h

all: gen_config $(TARGET)

$(TARGET): $(SRCS) boot_test.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

ifeq ($(EMU),tsim)
run: run_tsim
else
run: run_qemu
endif

run_tsim: $(TARGET)
	@echo "--- running boot (TSIM) ---"
	@timeout 60 $(TSIM_BIN) \
	    -MConfig $(TSIM_MCFG) \
	    -OConfig $(TSIM_OCFG) \
	    -s -H -disable-watchdog -x 0 \
	    -o $(TARGET) </dev/null 2>&1 | tee /tmp/boot_test.log ; \
	PASS=1; \
	if ! grep -q "$(SENTINEL_BSS)" /tmp/boot_test.log; then \
	    echo "BOOT: FAIL (sentinel '$(SENTINEL_BSS)' missing)"; \
	    PASS=0; \
	fi; \
	if ! grep -q "$(SENTINEL_BOOT)" /tmp/boot_test.log; then \
	    echo "BOOT: FAIL (sentinel '$(SENTINEL_BOOT)' missing)"; \
	    PASS=0; \
	fi; \
	if [ "$$PASS" = "1" ]; then echo "BOOT: PASS"; else exit 1; fi

run_qemu: $(TARGET)
	@echo "--- running boot integration test ---"
	@$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee /tmp/boot_test.log ; \
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
