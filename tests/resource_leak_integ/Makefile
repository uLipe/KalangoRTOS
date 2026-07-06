ARCH ?= tricore
ROOT := ../..
TARGET := resource_leak_test.elf

TEST_SRCS := ../test_printk.c resource_leak_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := "resource_leak: start" "resource_leak: PASS"

include ../integ_common.mk
