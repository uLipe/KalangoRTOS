CC      := tricore-elf-gcc

# ---------------------------------------------------------------------------
# Simulator selection
#
# TSIM_PATH is set by tools/dev.py when the host has the Infineon TSIM
# installed at /opt/Tools/TSIM-Tricore-instruction-set-simulator.
# If TSIM_PATH is empty or the binary is not executable, we fall back to QEMU.
# ---------------------------------------------------------------------------
TSIM_PATH ?=
TSIM_BIN  := $(TSIM_PATH)/bin/tsim16p_e
TSIM_MCFG := $(TSIM_PATH)/config/tc162/tc39xx/MConfig
TSIM_OCFG := $(TSIM_PATH)/config/tc162/tc39xx/OConfig

HAS_TSIM  := $(if $(TSIM_PATH),$(shell test -x "$(TSIM_BIN)" && echo 1 || echo 0),0)

QEMU      := qemu-system-tricore
MACHINE   := KIT_AURIX_TC277_TRB

# Default ELF targets
TARGET_QEMU := hello_qemu.elf
TARGET_TSIM := hello_tsim.elf

# ---------------------------------------------------------------------------
# Compiler / linker flags (shared between both variants)
# ---------------------------------------------------------------------------
CFLAGS  := -mcpu=tc39xx \
           -ffunction-sections \
           -fdata-sections \
           -ffreestanding \
           -fno-exceptions \
           -Wall -Wextra \
           -O0 -g

LDFLAGS := -T linker.ld \
           -mcpu=tc39xx \
           -nostartfiles \
           -Wl,--gc-sections

LIBS    := -lc -lgcc

SRCS_COMMON := startup.S main.c

# ---------------------------------------------------------------------------
# Build targets
# ---------------------------------------------------------------------------
all: $(TARGET_QEMU) $(TARGET_TSIM)

$(TARGET_QEMU): $(SRCS_COMMON) platform.c linker.ld
	$(CC) $(CFLAGS) $(SRCS_COMMON) platform.c $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

$(TARGET_TSIM): $(SRCS_COMMON) platform_tsim.c linker.ld
	$(CC) $(CFLAGS) $(SRCS_COMMON) platform_tsim.c $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

# ---------------------------------------------------------------------------
# Run targets
# ---------------------------------------------------------------------------

# Auto-select: prefer TSIM when available, fall back to QEMU.
ifeq ($(HAS_TSIM),1)
run: run_tsim
else
run: run_qemu
endif

run_qemu: $(TARGET_QEMU)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET_QEMU) \
	    -nographic \
	    -d guest_errors

run_tsim: $(TARGET_TSIM)
	@echo "--- TSIM $(shell basename $(TSIM_PATH)) ---"
	$(TSIM_BIN) \
	    -MConfig $(TSIM_MCFG) \
	    -OConfig $(TSIM_OCFG) \
	    -s \
	    -H \
	    -disable-watchdog \
	    -o $(TARGET_TSIM)

# ---------------------------------------------------------------------------
# Debug (QEMU + GDB only — TSIM has its own IDE-based debugger)
# ---------------------------------------------------------------------------
debug: $(TARGET_QEMU)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET_QEMU) \
	    -nographic \
	    -d guest_errors \
	    -gdb tcp::1234 -S &
	tricore-elf-gdb $(TARGET_QEMU) \
	    -ex "target remote :1234" \
	    -ex "break main" \
	    -ex "continue"

clean:
	rm -f $(TARGET_QEMU) $(TARGET_TSIM)

.PHONY: all run run_qemu run_tsim debug clean
