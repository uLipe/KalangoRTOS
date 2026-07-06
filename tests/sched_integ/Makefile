ARCH ?= tricore
ROOT := ../..
TARGET := sched_integ_test.elf
QEMU_TIMEOUT := 30

TEST_SRCS := ../test_printk.c sched_integ_test.c
SENTINELS := "sched_integ: start" "sched_integ: PASS"

include ../integ_common.mk

ifeq ($(ARCH),tricore)
TEST_SRCS += $(BOARD)/board_timer.c
endif
