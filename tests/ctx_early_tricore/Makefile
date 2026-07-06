ARCH ?= tricore
ROOT := ../..
TARGET := ctx_early_tricore_test.elf

INTEG_KERNEL_EXTRA_CFLAGS := -DULMK_CSA_CTX_EARLY_TEST=1
INTEG_KERNEL_EXTRA_SRCS := $(ROOT)/tests/ctx_early_tricore/ctx_early_tricore_kernel.c

TEST_SRCS := \
	../test_printk.c \
	ctx_early_tricore_test.c

SENTINELS := "ctx_early_tricore: PASS"

QEMU_TIMEOUT := 60

include ../integ_common.mk
