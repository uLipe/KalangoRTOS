CC      := tricore-elf-gcc
AR      := tricore-elf-ar
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
ROOT    := ../..
TARGET  := sched_integ_test.elf
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
    -T ../boot/boot_test.ld \
    -mcpu=tc39xx \
    -nostartfiles \
    -Wl,--gc-sections \
    -Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc

TEST_SRCS := \
    ../test_printk.c \
    sched_integ_test.c \
    $(CONSOLE_SRC)

SENTINELS := \
    "sched_integ: start" \
    "sched_integ: PASS"

QEMU_TIMEOUT := 30
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run clean gen_config

gen_config:
	@mkdir -p $(GEN_INC)/ulmk
	@printf '#ifndef ULMK_CONFIG_H\n#define ULMK_CONFIG_H\n' > $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_MAX_IRQ_BINDINGS 16\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#define ULMK_CONFIG_DEBUG_PRINTK     1\n' >> $(GEN_INC)/ulmk/config.h
	@printf '#endif\n' >> $(GEN_INC)/ulmk/config.h

all: gen_config $(TARGET)

$(TARGET): $(INTEG_KERNEL_LIB) $(TEST_SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(TEST_SRCS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running sched integration test (timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee /tmp/sched_integ_test.log ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" /tmp/sched_integ_test.log ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "SCHED INTEG TEST: PASS" ; \
	else \
	    echo "SCHED INTEG TEST: FAIL" ; \
	    cat /tmp/sched_integ_test.log ; \
	    exit 1 ; \
	fi

clean:
	rm -f $(TARGET) /tmp/sched_integ_test.log
