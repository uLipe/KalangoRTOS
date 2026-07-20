CASE_NAME := smp4_smoke
CASE_SRCS := root_thread.c
SENTINELS := "smp4_smoke: PASS"
FAIL_SENTINEL := "smp4_smoke: FAIL"
QEMU_TIMEOUT := 60
SMP := 1
SMP_N := 4
BOARD := boards/qemu_riscv_virt_smp4

include ../sdk_case.mk
