CASE_NAME := smp4_ipc_star
CASE_SRCS := root_thread.c
SENTINELS := "smp4_ipc_star: PASS"
FAIL_SENTINEL := "smp4_ipc_star: FAIL"
QEMU_TIMEOUT := 90
SMP := 1
SMP_N := 4
BOARD := boards/qemu_riscv_virt_smp4

include ../sdk_case.mk
