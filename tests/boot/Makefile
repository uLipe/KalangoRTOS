CC      := tricore-elf-gcc
AR      := tricore-elf-ar
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
ROOT    := ../..
TARGET  := boot_test.elf
GEN_INC := /tmp/gen

include ../integ_kernel.mk

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
    -DULMK_ARCH_IDLE_IS_WAIT=0 \
    -DULMK_ARCH_MPU_NUM_DPR=4 \
    -DULMK_TEST_BUILD=1

LDFLAGS := \
    -T boot_test.ld \
    -mcpu=tc39xx \
    -nostartfiles \
    -Wl,--gc-sections \
    -Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc

TEST_SRCS := \
    ../test_printk.c \
    boot_root_thread.c \
    $(CONSOLE_SRC)

SENTINEL_BSS  := boot_test: bss zero check — OK
SENTINEL_BOOT := BOOT OK

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ulmk
	@printf '#ifndef ULMK_CONFIG_H\n#define ULMK_CONFIG_H\n' > $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#endif\n' >> $(GEN_INC)/ulmk/config.h

all: gen_config $(TARGET)

$(TARGET): $(INTEG_KERNEL_LIB) $(TEST_SRCS) boot_test.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(TEST_SRCS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
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
	$(MAKE) integ_kernel_clean
