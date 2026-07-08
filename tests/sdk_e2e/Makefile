# tests/sdk_e2e/Makefile — standalone SDK consumer end-to-end test
#
# Unlike the other integration suites, this one does NOT compile the kernel
# sources directly.  It first builds the distributable SDK (two static libs +
# processed linker script + public headers) via tools/sdk_build.sh, then links
# a standalone application (sdk_test.c) against that SDK exactly as an external
# firmware project would.  This validates the shipped artefacts end to end.
#
# Runs inside the dev container (needs cmake, ninja and the cross toolchains).

ARCH ?= tricore
WS   := $(abspath $(CURDIR)/../..)
QEMU_TIMEOUT ?= 20

ifeq ($(ARCH),tricore)
CC        := tricore-elf-gcc
QEMU      := qemu-system-tricore
MACHINE   := KIT_AURIX_TC397B_TRB
QEMU_EXTRA :=
BOARD     := boards/qemu_tc3xx
ARCH_FLAGS := -mcpu=tc39xx
else ifeq ($(ARCH),riscv)
CC        := riscv-none-elf-gcc
QEMU      := qemu-system-riscv32
MACHINE   := virt
QEMU_EXTRA := -bios none -m 16M
BOARD     := boards/qemu_riscv_virt
ARCH_FLAGS := -march=rv32imac_zicsr_zifencei -mabi=ilp32
else ifeq ($(ARCH),arm)
# Cortex-M: default to the ARMv7-M board (mps2-an500); override with
#   make ARCH=arm ARM_BOARD=qemu_mps2_an505 ...
ARM_BOARD ?= qemu_mps2_an500
CC        := arm-none-eabi-gcc
QEMU      := qemu-system-arm
BOARD     := boards/$(ARM_BOARD)
ifeq ($(ARM_BOARD),qemu_mps2_an505)
MACHINE    := mps2-an505
ARCH_FLAGS := -mthumb -mcpu=cortex-m33 -mfloat-abi=softfp -mfpu=fpv5-sp-d16
else
MACHINE    := mps2-an500
ARCH_FLAGS := -mthumb -mcpu=cortex-m7 -mfloat-abi=softfp -mfpu=fpv5-sp-d16
endif
# -semihosting lets ulmk_sim_exit() (SYS_EXIT) stop the machine with a status.
QEMU_EXTRA := -semihosting
else
$(error unsupported ARCH '$(ARCH)' — use tricore, riscv or arm)
endif

BOARD_NAME := $(notdir $(BOARD))
TAG        := $(ARCH)_$(BOARD_NAME)_gcc
TOOLCHAIN  := $(WS)/cmake/toolchain-$(ARCH)-gcc.cmake

OUT   := $(CURDIR)/_out
BUILD := $(OUT)/build
SDK   := $(OUT)/ulmk

KERNEL_A := $(SDK)/lib/ulmk_kernel_$(TAG).a
BOARD_A  := $(SDK)/lib/ulmk_board_$(TAG).a
LD       := $(SDK)/linker/linker_$(TAG).ld

TARGET := sdk_test.elf
LOG    := /tmp/sdk_test_$(ARCH).log

SENTINELS := "SDK_TEST: begin" "SDK_TEST: PASS"

CFLAGS := \
	$(ARCH_FLAGS) \
	-ffreestanding \
	-Wall -Wextra -Wno-unused-parameter \
	-O2 -g \
	-I$(SDK)/include \
	-I$(SDK)/include/board

LDFLAGS := \
	-T $(LD) \
	$(ARCH_FLAGS) \
	-nostartfiles \
	-Wl,--gc-sections \
	-Wl,--no-warn-rwx-segments

TIMEOUT_CMD := timeout --kill-after=5 $(QEMU_TIMEOUT)

.PHONY: all run clean gen_config sdk

# The SDK build runs its own gen_config through CMake; nothing to do here.
gen_config:
	@true

sdk:
	@echo "--- building SDK ($(TAG)) ---"
	bash $(WS)/tools/sdk_build.sh \
		--toolchain $(TOOLCHAIN) \
		--chip-dir $(WS)/$(BOARD) \
		--arch $(ARCH) \
		--board-name $(BOARD_NAME) \
		--build-dir $(BUILD) \
		--out-dir $(SDK)

all: sdk $(TARGET)

$(TARGET): sdk sdk_test.c
	$(CC) $(CFLAGS) $(LDFLAGS) \
		sdk_test.c \
		-Wl,--start-group $(BOARD_A) $(KERNEL_A) -Wl,--end-group \
		-lc -lgcc \
		-o $@

run: all
	@echo "--- running $(TARGET) (ARCH=$(ARCH), timeout=$(QEMU_TIMEOUT)s) ---"
	@$(TIMEOUT_CMD) $(QEMU) \
	    -machine $(MACHINE) \
	    $(QEMU_EXTRA) \
	    -kernel $(TARGET) \
	    -nographic \
	    </dev/null 2>&1 | tee $(LOG) ; \
	FAIL=0 ; \
	for S in $(SENTINELS) ; do \
	    if grep -q "$$S" $(LOG) ; then \
	        echo "  [PASS] found: $$S" ; \
	    else \
	        echo "  [FAIL] missing: $$S" ; \
	        FAIL=1 ; \
	    fi ; \
	done ; \
	if grep -q "SDK_TEST: FAIL" $(LOG) ; then \
	    echo "  [FAIL] sdk_test reported FAIL" ; \
	    FAIL=1 ; \
	fi ; \
	if [ $$FAIL -eq 0 ]; then \
	    echo "SDK E2E TEST PASS: $(TARGET)" ; \
	else \
	    echo "SDK E2E TEST FAIL: $(TARGET)" ; \
	    cat $(LOG) ; \
	    exit 1 ; \
	fi

clean:
	rm -rf $(OUT) $(TARGET) $(LOG)
