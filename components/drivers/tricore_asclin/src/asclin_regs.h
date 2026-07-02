/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * asclin_regs.h — TC27x ASCLIN register map (private, not part of public API).
 *
 * Reference: Infineon TC27x User Manual Vol 2, Chapter 34 — ASCLIN.
 * All offsets and bit positions must be verified against the specific
 * device revision used.
 */
#ifndef ASCLIN_REGS_H
#define ASCLIN_REGS_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Base addresses — TC27x (TC27xD Rev 2.2)
 * -------------------------------------------------------------------------
 */
#define ASCLIN0_BASE  0xF0000600u
#define ASCLIN1_BASE  0xF0000700u
#define ASCLIN2_BASE  0xF0000800u
#define ASCLIN3_BASE  0xF0000900u

static const uintptr_t asclin_bases[4] = {
	ASCLIN0_BASE,
	ASCLIN1_BASE,
	ASCLIN2_BASE,
	ASCLIN3_BASE,
};

/* -------------------------------------------------------------------------
 * Register offsets
 * -------------------------------------------------------------------------
 */
#define ASCLIN_CLC_OFF          0x00u  /* Clock Control Register */
#define ASCLIN_IOCR_OFF         0x04u  /* I/O Control Register */
#define ASCLIN_TXFIFOCON_OFF    0x10u  /* TX FIFO Configuration Register */
#define ASCLIN_RXFIFOCON_OFF    0x14u  /* RX FIFO Configuration Register */
#define ASCLIN_BITCON_OFF       0x18u  /* Bit Configuration Register */
#define ASCLIN_FRAMECON_OFF     0x1Cu  /* Frame Control Register */
#define ASCLIN_DATCON_OFF       0x20u  /* Data Configuration Register */
#define ASCLIN_BRG_OFF          0x24u  /* Baud Rate Generation Register */
#define ASCLIN_FLAGS_OFF        0x38u  /* Flags Register */
#define ASCLIN_FLAGSCLEAR_OFF   0x40u  /* Flags Clear Register */
#define ASCLIN_TXDATA_OFF       0x4Cu  /* TX Data Register (write-only) */
#define ASCLIN_RXDATA_OFF       0x50u  /* RX Data Register (read-only) */
#define ASCLIN_CSR_OFF          0x54u  /* Clock Source Register */

#define ASCLIN_REG(base, off) \
	(*((volatile uint32_t *)((uintptr_t)(base) + (uint32_t)(off))))

/* -------------------------------------------------------------------------
 * CLC — Clock Control Register
 * -------------------------------------------------------------------------
 */
#define ASCLIN_CLC_DISR   (1u << 0)   /* 0 = enable module clock */
#define ASCLIN_CLC_DISS   (1u << 1)   /* 1 = module is disabled (read-only) */

/* -------------------------------------------------------------------------
 * CSR — Clock Source Register
 * bits [1:0] CLKSEL
 * -------------------------------------------------------------------------
 */
#define ASCLIN_CSR_CLKSEL_NOCLK  0u   /* no clock — use during config */
#define ASCLIN_CSR_CLKSEL_FA     1u   /* fA: fast peripheral bus */
#define ASCLIN_CSR_CLKSEL_FREF   3u   /* fOSC: oscillator reference */

/* -------------------------------------------------------------------------
 * BITCON — Bit Configuration Register
 *   bits [3:0]   OVERSAMPLING  (actual factor = value + 1; 15 → 16x)
 *   bits [11:8]  SAMPLEPOINT   (position within oversampling period)
 *   bits [27:16] PRESCALER     (actual divider = value + 1; 0 → /1)
 * -------------------------------------------------------------------------
 */
#define ASCLIN_BITCON_OS_SHIFT     0u
#define ASCLIN_BITCON_SP_SHIFT     8u
#define ASCLIN_BITCON_PRESC_SHIFT  16u
#define ASCLIN_BITCON_OS_16        15u  /* 16x oversampling */
#define ASCLIN_BITCON_SP_DEFAULT    9u  /* sample at 9/16 of bit period */
#define ASCLIN_BITCON_PRESC_1       0u  /* prescaler = 1 (no division) */

/* -------------------------------------------------------------------------
 * FRAMECON — Frame Control Register (UART 8N1 values)
 *   bits [1:0]   STOP  (0 = 1 stop bit)
 *   bit  [4]     PEN   (parity enable; 0 = none)
 *   bit  [16]    MSB   (0 = LSB first)
 *   bits [19:17] MODE  (0 = init, 1 = ASC/UART, 3 = LIN)
 * -------------------------------------------------------------------------
 */
#define ASCLIN_FRAMECON_STOP_1BIT  (0u << 0)
#define ASCLIN_FRAMECON_PAR_NONE   (0u << 4)
#define ASCLIN_FRAMECON_LSB_FIRST  (0u << 16)
#define ASCLIN_FRAMECON_MODE_INIT  (0u << 17)
#define ASCLIN_FRAMECON_MODE_ASC   (1u << 17)

/* -------------------------------------------------------------------------
 * DATCON — Data Configuration Register
 *   bits [3:0] DATLEN (data length - 1; 7 → 8-bit frames)
 * -------------------------------------------------------------------------
 */
#define ASCLIN_DATCON_8BIT  7u

/* -------------------------------------------------------------------------
 * TXFIFOCON / RXFIFOCON
 *   TXFIFOCON bits [1:0]: FLUSH(0), ENO(1) — enable TX output
 *   RXFIFOCON bits [1:0]: ENI(0), FLUSH(1) — enable RX input
 *   Both registers bits [10:8]: FILL — current FIFO fill level (read-only)
 * -------------------------------------------------------------------------
 */
#define ASCLIN_TXFIFOCON_FLUSH      (1u << 0)
#define ASCLIN_TXFIFOCON_ENO        (1u << 1)
#define ASCLIN_TXFIFOCON_FILL_SHIFT  8u
#define ASCLIN_TXFIFOCON_FILL_MASK  (0x7u << 8)

#define ASCLIN_RXFIFOCON_ENI        (1u << 0)
#define ASCLIN_RXFIFOCON_FLUSH      (1u << 1)
#define ASCLIN_RXFIFOCON_FILL_SHIFT  8u
#define ASCLIN_RXFIFOCON_FILL_MASK  (0x7u << 8)

#define ASCLIN_FIFO_DEPTH  8u   /* TC27x: 8-byte TX and RX FIFOs */

/* -------------------------------------------------------------------------
 * BRG — Baud Rate Generation Register
 *   bits [11:0]  DENOMINATOR (D)
 *   bits [27:16] NUMERATOR   (N)
 *
 * Baud rate formula (PRESCALER=1, OVERSAMPLING=16):
 *   baud = fA * N / (D * (PRESC+1) * (OS+1) * 2)
 *        = fA * N / (D * 32)
 *
 * Solve for N with fixed D = 3072:
 *   N = baud * D * 32 / fA  (round to nearest integer)
 * -------------------------------------------------------------------------
 */
#define ASCLIN_BRG_DENOM_SHIFT   0u
#define ASCLIN_BRG_NUMER_SHIFT  16u
#define ASCLIN_BRG_DENOM_DEFAULT 3072u

/* FLAGS — only bits used for polling (others exist for interrupt-driven use) */
#define ASCLIN_FLAGS_TXF  (1u << 0)  /* TX FIFO fill level event */
#define ASCLIN_FLAGS_RXF  (1u << 4)  /* RX FIFO fill level event */

#endif /* ASCLIN_REGS_H */
