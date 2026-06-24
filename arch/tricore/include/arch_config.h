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
