/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 / TC2xx architecture constants
 * Full specification: docs/arch_api_spec.md §4
 *
 * Included transitively via ul/arch.h; do not include directly.
 *
 * Board-specific overrides
 * ------------------------
 * All peripheral addresses are guarded by #ifndef so that a board's
 * Makefile can override them via -D flags without patching this file.
 *
 * Defaults target TC27x real silicon and the TSIM simulator with a TC27x
 * MConfig (boards/tsim_tc27x/MConfig).
 *
 * For the QEMU Linumiz fork (boards/qemu_tc27x) the following overrides
 * are required in the board Makefile:
 *
 *   -DUL_ARCH_STM0_BASE=0xF0000000u        (QEMU remaps STM0 here)
 *   -DUL_ARCH_SRC_STM0_SR0=0xF0038300u     (QEMU compressed IR slot 0xC0)
 *   -DUL_ARCH_SRC_SRE_BIT=10u              (QEMU IR format: SRE at bit 10)
 *   -DUL_ARCH_RAM_BASE=0x90000000u          (QEMU uses LMU space for RAM)
 *   -DUL_ARCH_IDLE_IS_WAIT=0               (NOP idle; no STM-driven wakeup)
 *   -DUL_ARCH_QEMU_VIRT_CONSOLE=1          (enable VIRT device DPR range)
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
 * PSW initial values for fabricated thread contexts.
 *
 * TC1.6.1 PSW bit layout:
 *   [11:10] IO  — privilege: 00=User-0, 01=User-1, 10=Supervisor
 *   [9]     IS  — interrupt stack indicator (0 = task stack)
 *   [8]     GW  — global address register write permission
 *   [7]     CDE — call depth count enable (0 = disabled)
 *   [6:0]   CDC — call depth counter
 *
 * CDE=1: call depth protection enabled (inherited from kernel PSW).
 * IS=0: thread uses its own stack (A10), not ISP.
 * GW=1: allow writes to small-data global registers (A0, A1, A8, A9).
 *
 * NOTE: ul_arch_ctx_init() reads the live kernel PSW and overrides IS=0
 * and CDC=0 at thread creation time, so these constants are for reference
 * and test/tooling use only.
 * ========================================================================= */
#define UL_ARCH_PSW_USER	0x00000180u	/* IO=0, IS=0, GW=1, CDE=1, CDC=0 */
#define UL_ARCH_PSW_DRIVER	0x00000580u	/* IO=1, IS=0, GW=1, CDE=1, CDC=0 */
#define UL_ARCH_PSW_SUPER	0x00000980u	/* IO=2, IS=0, GW=1, CDE=1, CDC=0 */

/* =========================================================================
 * CSA pool constraints
 * Each CSA frame is exactly 64 bytes; FCX/LCX encode segment + offset.
 * ========================================================================= */

#define UL_ARCH_CSA_FRAME_SIZE	64
#define UL_ARCH_CSA_MIN_COUNT	64	/* minimum frames in the pool */

/* =========================================================================
 * STM0 peripheral registers (TC27x AURIX)
 *
 * Default: TC27x hardware / TSIM (boards/tsim_tc27x/MConfig STM_BASE_ADDRESS).
 * Override for QEMU Linumiz: -DUL_ARCH_STM0_BASE=0xF0000000u
 * ========================================================================= */

#ifndef UL_ARCH_STM0_BASE
#define UL_ARCH_STM0_BASE	0xF0001000u	/* TC27x hardware / TSIM */
#endif
#define UL_ARCH_STM0_TIM0	(UL_ARCH_STM0_BASE + 0x010u)
#define UL_ARCH_STM0_CMP0	(UL_ARCH_STM0_BASE + 0x030u)
#define UL_ARCH_STM0_CMCON	(UL_ARCH_STM0_BASE + 0x038u)
#define UL_ARCH_STM0_ICR	(UL_ARCH_STM0_BASE + 0x03Cu)
#define UL_ARCH_STM0_ISCR	(UL_ARCH_STM0_BASE + 0x040u)

/*
 * Service Request Control for STM0 channel 0.
 *
 * TC27x hardware / TSIM:
 *   SRC_STM0SR0 = 0xF0038490 (SRC base 0xF0038000 + offset 0x490).
 *   SRC register layout: [7:0]=SRPN, [11:10]=TOS, [12]=SRE, [25]=SRR,
 *                        [26]=CLRR, [27]=SETR.
 *
 * QEMU Linumiz override:
 *   -DUL_ARCH_SRC_STM0_SR0=0xF0038300u  (IR slot 0xC0 × 4)
 *   -DUL_ARCH_SRC_SRE_BIT=10u           (QEMU IR format: SRE at bit 10)
 */
#ifndef UL_ARCH_SRC_STM0_SR0
#define UL_ARCH_SRC_STM0_SR0	0xF0038490u
#endif

#ifndef UL_ARCH_SRC_SRE_BIT
#define UL_ARCH_SRC_SRE_BIT	12u		/* TC27x: SRE at bit 12 */
#endif

/*
 * STM SRPN assigned to the kernel tick interrupt.
 * Priority 1 is the lowest non-disabled priority; preempts code at CCPN=0.
 */
#define UL_ARCH_TICK_SRPN	1u

/* SRC write value: SRPN=UL_ARCH_TICK_SRPN, TOS=0 (CPU0), SRE=1 */
#ifndef UL_ARCH_SRC_CONFIG_VAL
#define UL_ARCH_SRC_CONFIG_VAL	(UL_ARCH_TICK_SRPN | (1u << UL_ARCH_SRC_SRE_BIT))
#endif

/*
 * STM clock frequency (Hz).
 * TC27x default: f_SPB = 50 MHz.  Adjust for board PLL configuration.
 */
#ifndef UL_ARCH_STM_CLOCK_HZ
#define UL_ARCH_STM_CLOCK_HZ	50000000u
#endif

/* Ticks per microsecond at UL_ARCH_STM_CLOCK_HZ (integer, rounded) */
#define UL_ARCH_STM_TICKS_PER_US	(UL_ARCH_STM_CLOCK_HZ / 1000000u)

/* =========================================================================
 * CPU idle implementation
 *
 * Default: WAIT instruction — suspends the CPU until the next interrupt,
 * which is the correct low-power idle on TriCore silicon.
 *
 * Both QEMU Linumiz and TSIM standalone mode (-s) treat a CPU in WAIT state
 * as "no progress" and time out before the timer interrupt fires.  Use
 * -DUL_ARCH_IDLE_IS_WAIT=0 in all simulator builds (NOP busy-wait keeps the
 * instruction counter advancing so the simulator does not stall-detect).
 * ========================================================================= */
#ifndef UL_ARCH_IDLE_IS_WAIT
#define UL_ARCH_IDLE_IS_WAIT	1
#endif

/* =========================================================================
 * Memory map — board-specific TC27x layout
 * Used by ul_arch_mpu_init() to configure static DPR/CPR ranges.
 * ========================================================================= */

#ifndef UL_ARCH_FLASH_BASE
#define UL_ARCH_FLASH_BASE	0x80000000u	/* TC27x NC PFlash alias */
#endif
#define UL_ARCH_FLASH_SIZE	0x00200000u	/* 2 MiB */

#ifndef UL_ARCH_RAM_BASE
#define UL_ARCH_RAM_BASE	0x70000000u	/* TC27x DSPR0 / TSIM */
#endif
#ifndef UL_ARCH_RAM_SIZE
#define UL_ARCH_RAM_SIZE	0x00100000u	/* 1 MiB */
#endif

#define UL_ARCH_PERIPH_BASE	0xF0000000u	/* TC2xx peripheral bus */
#define UL_ARCH_PERIPH_SIZE	0x10000000u	/* 256 MiB */

/*
 * QEMU Linumiz virtual device (0xBF000000): putchar and exit registers.
 * Only used when UL_ARCH_QEMU_VIRT_CONSOLE is defined (QEMU builds).
 */
#ifndef UL_ARCH_QEMU_VIRT_CONSOLE
#define UL_ARCH_QEMU_VIRT_CONSOLE	0
#endif
#define UL_ARCH_QEMU_VIRT_BASE		0xBF000000u
#define UL_ARCH_QEMU_VIRT_SIZE		0x00001000u

/* =========================================================================
 * MPU CSFR addresses — TC2xx data sheet (CPU vol. §3.4)
 *
 * Data Protection Ranges:
 *   DPRn_L = 0xC000 + n*8   (lower bound, bits[31:3])
 *   DPRn_U = 0xC004 + n*8   (upper bound, bits[31:3])
 *
 * Code Protection Ranges:
 *   CPRn_L = 0xD000 + n*8
 *   CPRn_U = 0xD004 + n*8
 *
 * Enable registers (one per PRS, bit n = DPRn/CPRn enabled for that PRS):
 *   CPRE_0..3 = 0xE000..0xE00C  (code read enable)
 *   DPRE_0..3 = 0xE010..0xE01C  (data read enable)
 *   DPWE_0..3 = 0xE020..0xE02C  (data write enable)
 *   CPXE_0..3 = 0xE040..0xE04C  (code execute enable)
 *
 * PSW.PRS [13:12] selects the active PRS (0–3).
 * SYSCON.PROTEN = bit 1 at CSFR 0xFE14.
 * ========================================================================= */

/* DPR lower/upper at slot n */
#define UL_ARCH_CSFR_DPR_L(n)	(0xC000u + (n) * 8u)
#define UL_ARCH_CSFR_DPR_U(n)	(0xC004u + (n) * 8u)

/* CPR lower/upper at slot n */
#define UL_ARCH_CSFR_CPR_L(n)	(0xD000u + (n) * 8u)
#define UL_ARCH_CSFR_CPR_U(n)	(0xD004u + (n) * 8u)

/* Enable registers (constant CSFR addresses) */
#define UL_ARCH_CSFR_CPRE_0	0xE000u
#define UL_ARCH_CSFR_CPRE_1	0xE004u
#define UL_ARCH_CSFR_CPRE_2	0xE008u
#define UL_ARCH_CSFR_CPRE_3	0xE00Cu

#define UL_ARCH_CSFR_DPRE_0	0xE010u
#define UL_ARCH_CSFR_DPRE_1	0xE014u
#define UL_ARCH_CSFR_DPRE_2	0xE018u
#define UL_ARCH_CSFR_DPRE_3	0xE01Cu

#define UL_ARCH_CSFR_DPWE_0	0xE020u
#define UL_ARCH_CSFR_DPWE_1	0xE024u
#define UL_ARCH_CSFR_DPWE_2	0xE028u
#define UL_ARCH_CSFR_DPWE_3	0xE02Cu

#define UL_ARCH_CSFR_CPXE_0	0xE040u
#define UL_ARCH_CSFR_CPXE_1	0xE044u
#define UL_ARCH_CSFR_CPXE_2	0xE048u
#define UL_ARCH_CSFR_CPXE_3	0xE04Cu

/* SYSCON.PROTEN = bit 1: enables the protection system */
#define UL_ARCH_CSFR_SYSCON	0xFE14u
#define UL_ARCH_SYSCON_PROTEN	(1u << 1)

/*
 * Static DPR/CPR slot assignments:
 *   Slot 0: kernel — covers entire address space (PRS 0 R+W)
 *   Slot 1: peripheral space (PRS 0 R+W; driver threads via UL_MMAP_PERIPH)
 *   Slot 2: flash read (PRS 1 R — user threads need to read constants)
 *   Slot 3: QEMU virt device (only when UL_ARCH_QEMU_VIRT_CONSOLE=1)
 *   Slots 4–5: reserved
 *   Slots 6–17: per-thread dynamic regions (DPR); configured by mpu_switch()
 *
 *   CPR slot 0: all flash execute+read (enabled for PRS 0 and PRS 1)
 *   CPR slots 1–9: per-thread code regions (reserved for future use)
 *
 * PSW.PRS field [13:12]:
 *   0 = kernel PRS (used by root thread and kernel internals)
 *   1 = user PRS  (all non-kernel threads)
 */
#define UL_ARCH_MPU_KERNEL_DPR	0
#define UL_ARCH_MPU_PERIPH_DPR	1
#define UL_ARCH_MPU_FLASH_DPR	2
#define UL_ARCH_MPU_VIRT_DPR	3	/* QEMU virt console slot (conditional) */
#define UL_ARCH_MPU_RAM_DPR	4
#define UL_ARCH_MPU_USER_DPR_BASE 6
#define UL_ARCH_MPU_CPR_ALL	0

/* PSW.PRS value for non-kernel threads */
#define UL_ARCH_PRS_USER	1u

/* =========================================================================
 * Syscall ABI — register assignments for SYSCALL instruction
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
