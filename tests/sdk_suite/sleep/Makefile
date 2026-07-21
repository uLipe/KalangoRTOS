CASE_NAME := sleep
CASE_SRCS := root_thread.c
QEMU_TIMEOUT := 60
SENTINELS := \
	"sleep: start" \
	"sleep: thread A done" \
	"sleep: thread B done" \
	"sleep: thread C done" \
	"sleep: PASS"

include ../sdk_case.mk
