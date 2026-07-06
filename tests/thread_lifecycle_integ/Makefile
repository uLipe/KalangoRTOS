ARCH ?= tricore
ROOT := ../..
TARGET := thread_lifecycle_test.elf
QEMU_TIMEOUT := 60

TEST_SRCS := ../test_printk.c thread_lifecycle_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := "thread_lifecycle: start" "thread_lifecycle: PASS"

include ../integ_common.mk
