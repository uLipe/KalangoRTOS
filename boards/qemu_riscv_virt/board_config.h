/* SPDX-License-Identifier: MIT */
/*
 * boards/qemu_riscv_virt/board_config.h
 *
 * SoC constants for qemu-system-riscv32 -machine virt.
 * Consumed by arch/riscv via arch_config.h and board sources.
 */

#ifndef ULMK_BOARD_CONFIG_H
#define ULMK_BOARD_CONFIG_H

/* CLINT for MSIP soft-IRQ tests; tick stays on Goldfish RTC via PLIC. */
#ifndef ULMK_ARCH_HAVE_CLINT
#define ULMK_ARCH_HAVE_CLINT		1
#endif

#ifndef ULMK_ARCH_HAVE_CLIC
#define ULMK_ARCH_HAVE_CLIC		0
#endif

#ifndef ULMK_ARCH_HAVE_PLIC
#define ULMK_ARCH_HAVE_PLIC		1
#endif

#define ULMK_BOARD_PLIC_BASE		0x0C000000u

#ifndef ULMK_BOARD_CLINT_BASE
#define ULMK_BOARD_CLINT_BASE		0x02000000u
#endif

#define ULMK_BOARD_TIMER_RTC_BASE	0x00101000u
#define ULMK_BOARD_TIMER_RTC_MAP_SIZE	0x1000u
#define ULMK_BOARD_TIMER_PLIC_IRQ	11u
#define ULMK_BOARD_TIMER_HW_CLOCK_HZ	1000000000u

#ifndef ULMK_ARCH_HAVE_FPU
#define ULMK_ARCH_HAVE_FPU		0
#endif

#ifndef ULMK_ARCH_PMP_NUM
#define ULMK_ARCH_PMP_NUM		8
#endif

#ifndef ULMK_ARCH_IDLE_IS_WFI
#define ULMK_ARCH_IDLE_IS_WFI		0
#endif

#endif /* ULMK_BOARD_CONFIG_H */
