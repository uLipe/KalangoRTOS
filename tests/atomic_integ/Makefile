ARCH ?= tricore
ROOT := ../..
TARGET := atomic_integ_test.elf

TEST_SRCS := ../test_printk.c atomic_integ_test.c atomic_hook.c
ARCH_SKIP_BOARD_SVC := 1
TEST_EXTRA_SRCS :=
INTEG_KERNEL_EXTRA_CFLAGS := -DUL_KERNEL_PRE_ROOT_HOOK
TEST_EXTRA_SRCS :=
SENTINELS := \
	"atomic_integ: start" \
	"atomic_integ: phase1 PASS" \
	"atomic_integ: phase2 PASS" \
	"atomic_integ: PASS"

include ../integ_common.mk
