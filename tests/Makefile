CC      := tricore-elf-gcc
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC277_TRB
TARGET  := test.elf

# Platform files are shared with tools/hello — no duplication.
PLAT_DIR  := ../tools/hello
UNITY_DIR := /opt/unity

# EMU: tsim (default when TSIM_PATH is set) or qemu
TSIM_PATH ?=
TSIM_BIN  := $(TSIM_PATH)/bin/tsim16p_e
HAS_TSIM  := $(if $(TSIM_PATH),$(shell test -x "$(TSIM_BIN)" && echo 1 || echo 0),0)
ifeq ($(EMU),)
  ifeq ($(HAS_TSIM),1)
    EMU := tsim
  else
    EMU := qemu
  endif
endif

CFLAGS := -mcpu=tc27xx \
          -ffunction-sections \
          -fdata-sections \
          -ffreestanding \
          -fno-exceptions \
          -Wall -Wextra \
          -I$(UNITY_DIR) \
          -O0 -g

ifeq ($(EMU),tsim)
PLATFORM_SRC := $(PLAT_DIR)/platform_tsim.c
TSIM_MCFG    := ../boards/tsim_tc27x/MConfig
TSIM_OCFG    := ../boards/tsim_tc27x/OConfig
else
PLATFORM_SRC := $(PLAT_DIR)/platform.c
CFLAGS       += -DUL_ARCH_QEMU_VIRT_CONSOLE=1
endif

LDFLAGS := -T $(PLAT_DIR)/linker.ld \
           -mcpu=tc27xx \
           -nostartfiles \
           -Wl,--gc-sections

LIBS := -lc -lgcc

PLAT_SRCS := $(PLAT_DIR)/startup.S \
             $(PLATFORM_SRC)

UNITY_SRC := $(UNITY_DIR)/unity.c

# All test suite files.  Add new suites here as they are implemented.
TEST_SRCS := runner.c \
             test_dummy.c \
             test_linker.c \
             test_thread_api.c \
             test_ipc_api.c \
             test_notif_api.c \
             test_mem_api.c \
             test_irq_api.c \
             test_libul.c

SRCS := $(PLAT_SRCS) $(UNITY_SRC) $(TEST_SRCS)

all: $(TARGET)

$(TARGET): $(SRCS) $(PLAT_DIR)/linker.ld
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@
	tricore-elf-size $@

ifeq ($(EMU),tsim)
run: run_tsim
ci: run_tsim
else
run: run_qemu
ci: run_qemu
endif

run_tsim: $(TARGET)
	@timeout 60 $(TSIM_BIN) \
	    -MConfig $(TSIM_MCFG) \
	    -OConfig $(TSIM_OCFG) \
	    -s -H -disable-watchdog \
	    -o $(TARGET)

run_qemu: $(TARGET)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors

debug: $(TARGET)
	$(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    -d guest_errors \
	    -gdb tcp::1234 -S &
	tricore-elf-gdb $(TARGET) \
	    -ex "target remote :1234" \
	    -ex "break main" \
	    -ex "continue"

clean:
	rm -f $(TARGET)

.PHONY: all run run_tsim run_qemu ci debug clean
