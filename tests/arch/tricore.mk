# tests/arch/tricore.mk — TriCore integration-test toolchain

CC      := tricore-elf-gcc
AR      := tricore-elf-ar
QEMU    := qemu-system-tricore
MACHINE := KIT_AURIX_TC397B_TRB
QEMU_EXTRA :=
BOARD   := $(ROOT)/boards/qemu_tc3xx
TEST_LD := boot_test.ld

BOARD_EXTRA_SRCS :=

ARCH_CFLAGS := \
	-mcpu=tc39xx \
	-DULMK_ARCH_QEMU_VIRT_CONSOLE=1 \
	-DULMK_ARCH_SRC_STM0_SR0=0xF0038300u \
	-DULMK_ARCH_SRC_SRE_BIT=10u \
	-DULMK_ARCH_IDLE_IS_WAIT=0 \
	-DULMK_ARCH_MPU_NUM_DPR=4

ARCH_LDFLAGS := -mcpu=tc39xx

ARCH_KERNEL_SRCS := \
	$(ROOT)/arch/tricore/startup.S \
	$(ROOT)/arch/tricore/arch.c \
	$(ROOT)/arch/tricore/ctx_switch.S \
	$(ROOT)/arch/tricore/vectors.S \
	$(ROOT)/boards/qemu_tc3xx/qemu_printk_hook.c

ARCH_INCLUDE := -I$(ROOT)/arch/tricore/include
