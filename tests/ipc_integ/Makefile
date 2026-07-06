ARCH ?= tricore
ROOT := ../..
TARGET := ipc_integ_test.elf

TEST_SRCS := ../test_printk.c ipc_integ_test.c $(CONSOLE_SRC)
TEST_EXTRA_SRCS :=
SENTINELS := \
	"ipc_integ: start" \
	"ipc_integ: pp server replied" \
	"ipc_integ: ping-pong PASS" \
	"ipc_integ: mc server done" \
	"ipc_integ: mc client A PASS" \
	"ipc_integ: mc client B PASS" \
	"ipc_integ: mc client C PASS" \
	"ipc_integ: notif producer signalled" \
	"ipc_integ: notif consumer PASS" \
	"ipc_integ: rn server IPC path PASS" \
	"ipc_integ: rn server notif path PASS" \
	"ipc_integ: PASS"

include ../integ_common.mk
