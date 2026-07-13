CASE_NAME := sched_order
CASE_SRCS := root_thread.c
SENTINELS := \
	"sched_order: high step 1" \
	"sched_order: high step 2" \
	"sched_order: mid step 1" \
	"sched_order: mid step 2" \
	"sched_order: low step 1" \
	"sched_order: PASS"
QEMU_TIMEOUT := 30

include ../sdk_case.mk
