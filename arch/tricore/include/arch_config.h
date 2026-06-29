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
 * PSW initial values for fabricated thread contexts.
 *
 * TC1.6.1 PSW bit layout (confirmed from QEMU cpu.h / Ghidra / NuttX):
 *   [11:10] IO  — privilege: 00=User-0, 01=User-1, 10=Supervisor
 *   [9]     IS  — interrupt stack indicator (0 = task stack)
 *   [8]     GW  — global address register write permission
 *   [7]     CDE — call depth count enable (0 = disabled)
 *   [6:0]   CDC — call depth counter
 *
 * CDE=0: call depth protection disabled — safe for fabricated contexts.
 * IS=0: thread uses its own stack (A10), not ISP.
 * GW=1: allow writes to small-data global registers (A0, A1, A8, A9).
 * ========================================================================= */
/*
 * Reference PSW values for TC1.6.1 fabricated thread contexts.
 * These are NOT used directly by ul_arch_ctx_init — that function reads
 * the runtime kernel PSW via MFCR and strips IS/CDC before applying the IO
 * level, so it inherits GW and CDE from the live kernel state.
 *
 * These constants remain here for documentation and for any future use
 * outside of ctx_init (e.g. unit tests, tooling).
 *
 * Key constraints confirmed against QEMU Linumiz:
 *   IS  = 0  — QEMU starts with IS=1 (reset PSW = 0xB80); threads need IS=0
 *              or RFE into the fabricated context triggers a class-4 PSE trap.
 *   CDE = 1  — inherited from kernel; required for the QEMU peripheral check.
 *   CDC = 0  — fresh counter for the new thread.
 *   GW  = 1  — allow writes to small-data globals (A0/A1/A8/A9).
 */
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
 * Memory map — board-specific QEMU TC27x layout
 * Used by ul_arch_mpu_init() to configure static DPR/CPR ranges.
 * Update for real TC27x: FLASH_BASE = 0xA0000000, RAM_BASE = 0x70000000.
 * ========================================================================= */

#define UL_ARCH_FLASH_BASE	0x80000000u	/* QEMU KERNEL_FLASH origin */
#define UL_ARCH_FLASH_SIZE	0x00200000u	/* 2 MiB */
#define UL_ARCH_RAM_BASE	0x90000000u	/* QEMU KERNEL_RAM origin */
#define UL_ARCH_RAM_SIZE	0x00100000u	/* 1 MiB */
#define UL_ARCH_PERIPH_BASE	0xF0000000u	/* TC2xx peripheral bus */
#define UL_ARCH_PERIPH_SIZE	0x10000000u	/* 256 MiB */

/*
 * QEMU Linumiz tricore_testboard virtual device (0xBF000000).
 * Provides semihosting-style putchar (0x20), exit (0x28), etc.
 * QEMU enforces PRS-based write checks even without PROTEN=1, so this
 * range must be explicitly covered by a DPR for all PRS levels.
 */
#define UL_ARCH_QEMU_VIRT_BASE	0xBF000000u
#define UL_ARCH_QEMU_VIRT_SIZE	0x00001000u	/* 4 KiB */

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
 *   CPRE_0..3 = 0xE000, 0xE004, 0xE008, 0xE00C   (code read enable)
 *   DPRE_0..3 = 0xE010, 0xE014, 0xE018, 0xE01C   (data read enable)
 *   DPWE_0..3 = 0xE020, 0xE024, 0xE028, 0xE02C   (data write enable)
 *   CPXE_0..3 = 0xE040, 0xE044, 0xE048, 0xE04C   (code execute enable)
 *
 * PSW.PRS [13:12] selects the active PRS (0–3).
 * Note: the microkernel_book_tricore.md erroneously states [15:14]; the ISA
 * and QEMU source confirm bits [13:12].
 *
 * SYSCON.PROTEN = bit 1 at CSFR 0xFE14: set to enable the protection system.
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
 *   Slot 3: QEMU virt device (PRS 0+1 R+W — needed for ul_printk from any PRS)
 *   Slots 4–5: reserved
 *   Slots 6–17: per-thread dynamic regions (DPR); configured by mpu_switch()
 *
 *   CPR slot 0: all flash execute+read (enabled for PRS 0 and PRS 1)
 *   CPR slots 1–9: per-thread code regions (reserved for future use)
 *
 * PSW.IO field for privilege levels:
 *   0 = User-0 (IO=0), 1 = User-1/Driver (IO=1), 2 = Supervisor (IO=2)
 *
 * PSW.PRS field [13:12]:
 *   0 = kernel PRS (used by root thread and kernel internals)
 *   1 = user PRS  (all non-kernel threads)
 */
#define UL_ARCH_MPU_KERNEL_DPR	0	/* DPR 0: entire addr-space (kernel + user) */
#define UL_ARCH_MPU_PERIPH_DPR	1	/* DPR 1: peripheral region */
#define UL_ARCH_MPU_FLASH_DPR	2	/* DPR 2: flash read */
#define UL_ARCH_MPU_VIRT_DPR	3	/* DPR 3: QEMU virt console */
/*
 * DPR 4: shared RAM — effective only on real TC277 (18 DPR).
 * QEMU's linumiz TC277 model silently ignores MTCR to CSFR 0xC020/0xC024,
 * so DPR 4 bounds read back as 0.  PRS-1 RAM access is instead granted via
 * DPR 0 (full range) in the QEMU build.  On real hardware with 18 DPRs,
 * DPR 4 and above provide per-domain fine-grained isolation.
 */
#define UL_ARCH_MPU_RAM_DPR	4	/* DPR 4: shared RAM (real HW only) */
#define UL_ARCH_MPU_USER_DPR_BASE 6	/* DPR 6..17: per-thread dynamic */
#define UL_ARCH_MPU_CPR_ALL	0	/* CPR 0: all flash, execute+read */

/* PSW.PRS value for non-kernel threads */
#define UL_ARCH_PRS_USER	1u

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
