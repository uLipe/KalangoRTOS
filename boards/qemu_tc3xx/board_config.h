/* SPDX-License-Identifier: MIT */
/*
 * boards/qemu_tc3xx/board_config.h
 *
 * SoC / platform constants for QEMU KIT_AURIX_TC397B_TRB (Linumiz).
 * Consumed by arch/tricore via arch_config.h and by board sources.
 *
 * Real silicon boards supply their own board_config.h with TC27x addresses.
 * Future: generated from board DTS (reg = <base size>).
 */

#ifndef ULMK_BOARD_CONFIG_H
#define ULMK_BOARD_CONFIG_H

/* ── IRQ: AURIX Service Request (SRC) block ─────────────────────────────── */

#define ULMK_BOARD_SRC_BASE		0xF0038000u
#define ULMK_BOARD_SRC_SRE_BIT		10u	/* QEMU tc4x_mode=0 */

/* STM0 compare-match → SRPN 1 (board_timer.c) */
#define ULMK_BOARD_SRC_STM0_SR0		0xF0038300u	/* IR slot 0xC0 */

/* ── Timer peripheral (STM0) ───────────────────────────────────────────── */

#define ULMK_BOARD_STM0_BASE		0xF0001000u

/* ── Memory map (MPU coarse regions; must match memory.ld ORIGIN) ──────── */

#define ULMK_BOARD_FLASH_BASE		0x80000000u
#define ULMK_BOARD_FLASH_SIZE		0x00200000u
#define ULMK_BOARD_RAM_BASE		0x70000000u
#define ULMK_BOARD_RAM_SIZE		0x0003C000u	/* 240 KiB DSPR */
#define ULMK_BOARD_PERIPH_BASE		0xF0000000u
#define ULMK_BOARD_PERIPH_SIZE		0x10000000u

/* ── QEMU Linumiz virt console (MMIO putchar/exit) ─────────────────────── */

#define ULMK_BOARD_HAVE_VIRT_CONSOLE	1
#define ULMK_BOARD_VIRT_CONSOLE_BASE	0xBF000000u
#define ULMK_BOARD_VIRT_CONSOLE_SIZE	0x00001000u

/* ── Arch build quirks for this emulation target ───────────────────────── */

#define ULMK_BOARD_IDLE_IS_WAIT		0	/* NOP idle — QEMU WAIT stalls */
#define ULMK_BOARD_MPU_NUM_DPR		4
#define ULMK_BOARD_MPU_NUM_CPR		4

/* TriCore ISA: QEMU TC397B models TC1.6.2 (TC3xx). */
#define ULMK_BOARD_TRICORE_ISA_MAJOR	1
#define ULMK_BOARD_TRICORE_ISA_MINOR	6
#define ULMK_BOARD_TRICORE_ISA_PATCH	2

#endif /* ULMK_BOARD_CONFIG_H */
