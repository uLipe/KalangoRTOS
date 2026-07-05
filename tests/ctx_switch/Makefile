CC      := tricore-elf-gcc
AR      := tricore-elf-ar
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
ROOT    := ../..
TARGET  := ctx_switch_test.elf
STRESS  := ctx_stress_test.elf
GEN_INC := /tmp/gen

include ../integ_kernel.mk
include ../integ_gen_config.mk

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

BASIC_TEST_SRCS := \
    ../test_printk.c \
    ctx_switch_root_thread.c \
    $(CONSOLE_SRC)

STRESS_TEST_SRCS := \
    ../test_printk.c \
    ctx_switch_stress.c \
    $(CONSOLE_SRC)

SENTINELS        := "switching to root thread" "ROOT THREAD RUNNING"
STRESS_SENTINELS := "ctx_stress: start" "ctx_stress: PASS"

QEMU_TIMEOUT := 120
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run stress clean

all: gen_config $(TARGET) $(STRESS)

$(TARGET): $(INTEG_KERNEL_LIB) $(BASIC_TEST_SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(BASIC_TEST_SRCS) $(LIBS) -o $@
	tricore-elf-size $@

$(STRESS): $(INTEG_KERNEL_LIB) $(STRESS_TEST_SRCS) ../boot/boot_test.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(STRESS_TEST_SRCS) $(LIBS) -o $@
	tricore-elf-size $@

run: $(TARGET)
	@echo "--- running ctx_switch integration test (timeout=$(QEMU_TIMEOUT)s) ---"
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
