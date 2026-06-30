CC      := cc

# ---------------------------------------------------------------------------
# Simulator selection: EMU=tsim (default when TSIM_PATH is set) or EMU=qemu
# ---------------------------------------------------------------------------
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
TARGET  := thread_unit_test

ROOT    := ../..
STUBINC := include

CFLAGS := \
    -std=c99 \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(STUBINC) \
    -I$(ROOT) \
    -g -O0

# Per-simulator console source and arch overrides
ifeq ($(EMU),tsim)
CONSOLE_SRC := $(ROOT)/boards/tsim_tc27x/tsim_console.c
CFLAGS      += -DUL_ARCH_IDLE_IS_WAIT=1
else
CONSOLE_SRC := $(ROOT)/boards/qemu_tc27x/qemu_console.c
CFLAGS      += -DUL_ARCH_QEMU_VIRT_CONSOLE=1
CFLAGS      += -DUL_ARCH_STM0_BASE=0xF0000000u
CFLAGS      += -DUL_ARCH_SRC_STM0_SR0=0xF0038300u
CFLAGS      += -DUL_ARCH_SRC_SRE_BIT=10u
CFLAGS      += -DUL_ARCH_RAM_BASE=0x90000000u
CFLAGS      += -DUL_ARCH_IDLE_IS_WAIT=0
endif

SRCS := \
    thread_unit_test.c \
    $(ROOT)/kernel/thread/thread.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

ifeq ($(EMU),tsim)
run: run_tsim
else
run: run_qemu
endif

run_tsim: $(TARGET)
	@echo "--- running thread_unit tests ---"
	@./$(TARGET) && echo "THREAD UNIT TEST: PASS" || \
	    { echo "THREAD UNIT TEST: FAIL"; exit 1; }

run_qemu: $(TARGET)
	@echo "--- running thread unit tests ---"
	@./$(TARGET) && echo "THREAD UNIT TEST: PASS" || \
	    { echo "THREAD UNIT TEST: FAIL"; exit 1; }

clean:
	rm -f $(TARGET)
