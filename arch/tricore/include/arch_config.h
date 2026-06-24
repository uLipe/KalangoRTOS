/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 / TC2xx architecture constants
 * Full specification: docs/arch_api_spec.md §4
 *
 * Included transitively via ul/arch.h; do not include directly.
 */

#ifndef UL_ARCH_TRICORE_CONFIG_H
#define UL_ARCH_TRICORE_CONFIG_H

/* =========================================================================
 * MPU hardware limits (TC2xx data sheet, DCON0/DPRE/DPWE registers)
 * ========================================================================= */

#define UL_ARCH_NUM_DPR		18	/* data protection ranges */
#define UL_ARCH_NUM_CPR		10	/* code protection ranges */
#define UL_ARCH_NUM_PRS		 4	/* protection register sets */

/*
 * Max MPU regions tracked per thread (code + data combined).
 * Must not exceed UL_ARCH_NUM_DPR + UL_ARCH_NUM_CPR.
 */
#define UL_ARCH_MAX_REGIONS	12

/*
 * Minimum alignment for MPU region base and size.
 * TriCore DPR/CPR granularity is 8 bytes; we use 64 to match CSA frames.
 */
#define UL_ARCH_REGION_ALIGN	64

/* =========================================================================
 * PSW initial values for new threads
 * TriCore PSW register layout:
 *   [31:26] CDE  [25] = 1 (CDC high bit)
 *   [13:12] IO   [11:0] CDC/CDC-lower
 * ========================================================================= */

/*
 * PSW.IO field is at bits[9:8]: 00=user, 01=driver, 10=supervisor.
 * CDE=bit11=1 enables call-depth error trap.  IS=bit7=1 selects ISP stack.
 *
 * UL_ARCH_PSW_USER   = IO=0, IS=0, CDE=1
 * UL_ARCH_PSW_DRIVER = IO=1, IS=0, CDE=1
 * UL_ARCH_PSW_SUPER  = IO=2, IS=0, CDE=1  (kernel-internal contexts)
 */
#define UL_ARCH_PSW_USER	0x00000880u
#define UL_ARCH_PSW_DRIVER	0x00000980u
#define UL_ARCH_PSW_SUPER	0x00000A80u

/* =========================================================================
 * CSA pool constraints
 * Each CSA frame is exactly 64 bytes; FCX/LCX encode segment + offset.
 * ========================================================================= */

#define UL_ARCH_CSA_FRAME_SIZE	64
#define UL_ARCH_CSA_MIN_COUNT	64	/* minimum frames in the pool */

/* =========================================================================
 * STM0 peripheral registers (TC2xx AURIX)
 *
 * NOTE: The QEMU Linumiz fork maps STM0 at 0xF0000000 (older revision).
 * Real TC27x hardware and the upstream QEMU fix use 0xF0001000.
 * Define UL_ARCH_STM0_BASE to 0xF0001000 when targeting real hardware.
 * ========================================================================= */

#define UL_ARCH_STM0_BASE	0xF0000000u	/* STM0 base (QEMU Linumiz fork) */
#define UL_ARCH_STM0_TIM0	(UL_ARCH_STM0_BASE + 0x010u)
#define UL_ARCH_STM0_CMP0	(UL_ARCH_STM0_BASE + 0x030u)
#define UL_ARCH_STM0_CMCON	(UL_ARCH_STM0_BASE + 0x038u)
#define UL_ARCH_STM0_ICR	(UL_ARCH_STM0_BASE + 0x03Cu)
#define UL_ARCH_STM0_ISCR	(UL_ARCH_STM0_BASE + 0x040u)

/*
 * Service Request Control for STM0 channel 0.
 *
 * The QEMU/EFS Linumiz IR maps each device to a compressed SRC slot index.
 * STM0 SR0 uses slot 0xC0 (TC27X_SRC_STM0_SR0 in the SoC QEMU driver).
 * Address = IR_SRC_BASE (0xF0038000) + 0xC0 * 4 = 0xF0038300.
 *
 * NOTE: Real TC27x hardware has SRC_STM0SR0 at 0xF0038490. The QEMU IR
 * uses a compressed index space that does NOT mirror hardware addresses.
 *
 * Linumiz IR SRC field layout for TC27x (tc4x_mode=0 in struct at offset 0x588):
 *   [7:0]  = SRPN  (service request priority number)
 *   [10]   = SRE   (service request enable; irq_evaluate checks this bit when tc4x_mode=0)
 *   [13:11]= TOS   (target CPU index; 0 = CPU0)
 *   [24]   = SRR   (set by irq_handler when interrupt is raised)
 *   [25]   = CLRR  (write 1 to clear SRR)
 *   [26]   = SETR  (write 1 to software-trigger SRR)
 *
 * tc4x_mode=1 shifts SRE to bit 23 and TOS to bits [15:12], but TC27x uses tc4x_mode=0.
 */
#define UL_ARCH_SRC_STM0_SR0	0xF0038300u

/* SRC write value: SRPN=UL_ARCH_TICK_SRPN, SRE=bit10=1, TOS=0 (CPU0) */
#define UL_ARCH_SRC_CONFIG_VAL	(UL_ARCH_TICK_SRPN | (1u << 10))

/*
 * STM SRPN assigned to the kernel tick interrupt.
 * Priority 1 is the lowest non-disabled priority; preempts code at CCPN=0.
 */
#define UL_ARCH_TICK_SRPN	1u

/*
 * STM clock frequency.  Derived from f_SPB; QEMU Linumiz sets it to 50 MHz
 * (confirmed from qemu_clock_set call: period = 0x1400000000 in 2^-32 ns
 * units = 20 ns/tick = 50 MHz).  Adjust for real hardware PLL configuration.
 */
#define UL_ARCH_STM_CLOCK_HZ	50000000u

/* Ticks per microsecond at UL_ARCH_STM_CLOCK_HZ (integer, rounded) */
#define UL_ARCH_STM_TICKS_PER_US	(UL_ARCH_STM_CLOCK_HZ / 1000000u)

/* =========================================================================
 * Syscall ABI — register assignments for SYSCALL instruction
 * Syscall number in d15; arguments in d4–d7; return value in d2.
 * ========================================================================= */

#define UL_ARCH_SYSCALL_NR_REG	 "d15"
#define UL_ARCH_SYSCALL_ARG0_REG "d4"
#define UL_ARCH_SYSCALL_ARG1_REG "d5"
#define UL_ARCH_SYSCALL_ARG2_REG "d6"
#define UL_ARCH_SYSCALL_ARG3_REG "d7"
#define UL_ARCH_SYSCALL_RET_REG  "d2"

/* =========================================================================
 * TriCore CSFR addresses used by the arch port
 * ========================================================================= */

#define UL_ARCH_CSFR_BTV	0xFE24u	/* Base Trap Vector */
#define UL_ARCH_CSFR_BIV	0xFE20u	/* Base Interrupt Vector */
#define UL_ARCH_CSFR_ISP	0xFE28u	/* Interrupt Stack Pointer */
#define UL_ARCH_CSFR_ICR	0xFE2Cu	/* Interrupt Control Register */
#define UL_ARCH_CSFR_FCX	0xFE38u	/* Free Context Pointer */
#define UL_ARCH_CSFR_LCX	0xFE3Cu	/* Last Context Pointer */
#define UL_ARCH_CSFR_PCXI	0xFE00u	/* Previous Context Pointer */
#define UL_ARCH_CSFR_PSW	0xFE04u	/* Program Status Word */

#endif /* UL_ARCH_TRICORE_CONFIG_H */
