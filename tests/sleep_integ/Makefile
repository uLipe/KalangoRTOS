ARCH ?= tricore
ROOT := ../..
TARGET := sleep_integ_test.elf

TEST_SRCS := \
	../test_printk.c \
	sleep_integ_test.c

SENTINELS := \
	"sleep_integ: start" \
	"sleep_integ: thread A alive (sleep 10)" \
	"sleep_integ: thread A done" \
	"sleep_integ: thread B done" \
	"sleep_integ: thread C done" \
	"sleep_integ: PASS"

include ../integ_common.mk

ifeq ($(ARCH),tricore)
TEST_SRCS += $(BOARD)/board_timer.c
endif

clean:
	rm -f $(TARGET) /tmp/$(TARGET:.elf=.log)
	$(MAKE) integ_kernel_clean
