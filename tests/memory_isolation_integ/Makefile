ARCH ?= tricore
ROOT := ../..
TARGET := memory_isolation_test.elf
QEMU_TIMEOUT := 90

TEST_SRCS := ../test_printk.c memory_isolation_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := \
	"mem_iso: start" \
	"mem_iso: scenario 1 (anon mmap) PASS" \
	"mem_iso: scenario 2 (grant read) PASS" \
	"mem_iso: scenario 3 (MPU fault) P" \
	"mem_iso: scenario 4 (kernel exec fault) P" \
	"mem_iso: scenario 5 (kernel data fault) P" \
	"MEMORY ISOLATION TEST: PASS"

include ../integ_common.mk
