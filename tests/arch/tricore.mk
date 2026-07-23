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
	-mcpu=tc39xx

ARCH_LDFLAGS := -mcpu=tc39xx

ARCH_KERNEL_SRCS := \
	$(ROOT)/arch/tricore/startup.S \
	$(ROOT)/arch/tricore/board_wdt_early_stub.S \
	$(ROOT)/arch/tricore/arch.c \
	$(ROOT)/arch/tricore/smp.c \
	$(ROOT)/arch/tricore/ctx_switch.S \
	$(ROOT)/arch/tricore/vectors.S \
	$(ROOT)/boards/qemu_tc3xx/qemu_printk_hook.c

ARCH_INCLUDE := -I$(ROOT)/arch/tricore/include
