ARCH ?= tricore
ROOT := ../..
TARGET := boot_test.elf

TEST_SRCS := \
	../test_printk.c \
	boot_root_thread.c

TEST_EXTRA_SRCS :=
SENTINELS := \
	"boot_test: bss zero check — OK" \
	"boot_test: root thread reached — BOOT OK"

include ../integ_common.mk
