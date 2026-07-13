ARCH ?= tricore
ROOT := ../../../..
TARGET := ctx_early_tricore_test.elf

INTEG_KERNEL_EXTRA_CFLAGS := -DULMK_CSA_CTX_EARLY_TEST=1
INTEG_KERNEL_EXTRA_SRCS := $(CURDIR)/ctx_early_tricore_kernel.c

TEST_SRCS := \
	$(ROOT)/tests/test_printk.c \
	ctx_early_tricore_test.c

SENTINELS := "ctx_early_tricore: PASS"

QEMU_TIMEOUT := 60

include $(ROOT)/tests/integ_common.mk
