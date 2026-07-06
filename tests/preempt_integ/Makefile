ARCH ?= tricore
ROOT := ../..
TARGET := preempt_integ_test.elf

TEST_SRCS := ../test_printk.c preempt_integ_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := "preempt_integ: start" "preempt_integ: PASS"

include ../integ_common.mk
