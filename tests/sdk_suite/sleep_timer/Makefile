CASE_NAME := sleep_timer
CASE_SRCS := root_thread.c
SENTINELS := \
	"sleep_timer: start" \
	"sleep_timer: thread A alive (sleep 10)" \
	"sleep_timer: thread A done" \
	"sleep_timer: thread B done" \
	"sleep_timer: thread C done" \
	"sleep_timer: PASS"
QEMU_TIMEOUT := 60

include ../sdk_case.mk
