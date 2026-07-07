# tests/integ_common.mk — shared integration-test build rules
# Include after: ROOT, TARGET, TEST_SRCS, SENTINELS, and optional BOARD_EXTRA_SRCS

ARCH    ?= tricore
GEN_INC ?= /tmp/gen
INTEG_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

include $(INTEG_DIR)arch/$(ARCH).mk
include $(INTEG_DIR)integ_kernel.mk

ifneq ($(ARCH_TEST_IO),)
TEST_SRCS := $(filter-out %test_printk.c,$(TEST_SRCS))
TEST_SRCS += $(ARCH_TEST_IO)
endif

.DEFAULT_GOAL := all

CONSOLE_SRC := $(BOARD)/qemu_console.c
TEST_LD_PATH := $(INTEG_DIR)boot/$(TEST_LD)

ifneq ($(ARCH),tricore)
ifneq ($(ARCH_SKIP_BOARD_SVC),1)
TEST_SRCS := $(filter-out %qemu_console.c,$(TEST_SRCS))
TEST_SRCS += \
	$(BOARD)/board_console.c \
	$(BOARD)/board_timer.c \
	$(BOARD)/board_services.c \
	$(CONSOLE_SRC)
endif
endif

TEST_SRCS += $(TEST_EXTRA_SRCS)

# Common userspace thread entry — compiled as a test object (outside
# libtest_kernel.a) so it lands in .user_text, executable by user threads.
TEST_SRCS += $(ROOT)/kernel/init/user_entry.c

CFLAGS := \
	$(ARCH_CFLAGS) \
	-ffreestanding \
	-ffunction-sections \
	-fdata-sections \
	-Wall -Wextra \
	-Wno-unused-parameter \
	-DULMK_KERNEL_BUILD \
	-I$(ROOT)/include \
	$(ARCH_INCLUDE) \
	-I$(BOARD) \
	-I$(ROOT) \
	-I$(GEN_INC) \
	-O0 -g \
	-DULMK_TEST_BUILD=1

LDFLAGS := \
	-T $(TEST_LD_PATH) \
	$(ARCH_LDFLAGS) \
	-nostartfiles \
	-Wl,--gc-sections \
	-Wl,--no-warn-rwx-segments

LIBS := -lc -lgcc
QEMU_TIMEOUT ?= 120
TIMEOUT_CMD  := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run clean gen_config

gen_config:
	@python3 $(ROOT)/tools/gen_config.py \
		--out-dir $(GEN_INC) \
		--board-config $(BOARD)/board_config.h

all: gen_config $(TARGET)

$(TARGET): $(INTEG_KERNEL_LIB) $(TEST_SRCS) $(TEST_LD_PATH)
	$(CC) $(CFLAGS) $(LDFLAGS) -Wl,--whole-archive $(INTEG_KERNEL_LIB) -Wl,--no-whole-archive $(TEST_SRCS) $(LIBS) -o $@

run: $(TARGET)
	@echo "--- running $(TARGET) (ARCH=$(ARCH), timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    $(QEMU_EXTRA) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee /tmp/$(TARGET:.elf=.log) ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" /tmp/$(TARGET:.elf=.log) ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "INTEG TEST PASS: $(TARGET)" ; \
	else \
	    echo "INTEG TEST FAIL: $(TARGET)" ; \
	    cat /tmp/$(TARGET:.elf=.log) ; \
	    exit 1 ; \
	fi

clean:
	rm -f $(TARGET) /tmp/$(TARGET:.elf=.log)
	$(MAKE) integ_kernel_clean
