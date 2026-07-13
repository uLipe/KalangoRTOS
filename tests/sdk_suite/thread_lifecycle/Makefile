CASE_NAME := thread_lifecycle
CASE_SRCS := root_thread.c
SENTINELS := \
	"thread_lifecycle: start" \
	"thread_lifecycle: kill target never ran" \
	"thread_lifecycle: PASS"
QEMU_TIMEOUT := 40

include ../sdk_case.mk
