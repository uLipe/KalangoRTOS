ARCH ?= tricore
ROOT := ../..
TARGET := irq_integ_test.elf

ifeq ($(ARCH),riscv)
ULMK_RISCV_IRQ_INTEG := 1
endif

TEST_SRCS := ../test_printk.c irq_integ_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := \
	"irq_integ: start" \
	"irq_integ: server ready, waiting for IRQs" \
	"irq_integ: basic IRQ delivery PASS" \
	"irq_integ: IRQ disable PASS" \
	"irq_integ: IRQ re-enable PASS" \
	"irq_integ: PASS"

include ../integ_common.mk
