CASE_NAME := irq_attach
CASE_SRCS := root_thread.c
SENTINELS := "irq_attach: start" "irq_attach: PASS"
QEMU_TIMEOUT := 40
# Requires ULMK_CONFIG_IRQ_ATTACH=1 (default OFF in the kernel).
IRQ_ATTACH := 1

include ../sdk_case.mk
