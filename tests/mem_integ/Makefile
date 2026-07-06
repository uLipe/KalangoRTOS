ARCH ?= tricore
ROOT := ../..
TARGET := mem_integ_test.elf
QEMU_TIMEOUT := 60

TEST_SRCS := ../test_printk.c mem_integ_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := "mem_integ: start" "mem_integ: PASS"

include ../integ_common.mk
