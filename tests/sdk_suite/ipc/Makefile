CASE_NAME := ipc
CASE_SRCS := root_thread.c
SENTINELS := \
	"ipc: start" \
	"ipc: ping-pong PASS" \
	"ipc: notif consumer PASS" \
	"ipc: PASS"
QEMU_TIMEOUT := 40

include ../sdk_case.mk
