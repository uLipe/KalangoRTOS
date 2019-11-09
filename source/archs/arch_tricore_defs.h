#pragma once

/*
 * TriCore TC1.6 / TC1.6.1 (AURIX TC2xx) / TC1.6.2 (AURIX TC3xx) definitions.
 *
 * Context Save Areas (CSAs):
 *   Upper context (saved on CALL/trap/interrupt): PCXI, PSW, A10-A15, D8-D15
 *   Lower context (saved on BISR/SVLCX): PCXI, A11(PC), A2-A7, D0-D7
 *
 * CSA is a 16-word (64-byte) block aligned on a 64-byte boundary.
 * CSA link word encoding: bits[19:16] = segment (addr[31:28] >> 12),
 *                          bits[15:0]  = offset  (addr[25:6]  >> 6)
 *
 * task->stackpointer stores a CSA link (uint32_t) to the task's lower context
 * CSA, which chains to the upper context CSA.
 */

/* ---------------------------------------------------------------------------
 * Core Special Function Register (CSFR) addresses
 * --------------------------------------------------------------------------- */
#ifdef __ASSEMBLER__
#define TRICORE_CSFR_PCXI   0xFE00
#define TRICORE_CSFR_PSW    0xFE04
#define TRICORE_CSFR_PC     0xFE08
#define TRICORE_CSFR_ISP    0xFE28
#define TRICORE_CSFR_ICR    0xFE2C
#define TRICORE_CSFR_FCX    0xFE38
#define TRICORE_CSFR_LCX    0xFE3C
#define TRICORE_CSFR_BIV    0xFE20
#define TRICORE_CSFR_BTV    0xFE24
#define TRICORE_CSFR_SYSCON 0xFF00
#else
#define TRICORE_CSFR_PCXI   0xFE00U
#define TRICORE_CSFR_PSW    0xFE04U
#define TRICORE_CSFR_PC     0xFE08U
#define TRICORE_CSFR_ISP    0xFE28U
#define TRICORE_CSFR_ICR    0xFE2CU
#define TRICORE_CSFR_FCX    0xFE38U
#define TRICORE_CSFR_LCX    0xFE3CU
#define TRICORE_CSFR_BIV    0xFE20U
#define TRICORE_CSFR_BTV    0xFE24U
#define TRICORE_CSFR_SYSCON 0xFF00U
#endif

/* ---------------------------------------------------------------------------
 * PSW (Program Status Word) bits — TC1.6.1 layout (AURIX TC2xx / QEMU tc2x)
 * bits[6:0]  = CDC (call depth counter)
 * bit[7]     = CDE (call depth count enable; 0 = disabled)
 * bit[8]     = GW  (global address register write permission)
 * bits[11:10]= IO  (privilege level: 0b10 = supervisor)
 *
 * All constants are defined without C-specific suffixes so they are also
 * valid in GAS (.S files processed with -x assembler-with-cpp).
 * --------------------------------------------------------------------------- */
#define TRICORE_PSW_IO_SUPERVISOR   0x800    /* bits[11:10] = 0b10 = supervisor */
#define TRICORE_PSW_GW              0x100    /* bit 8 = global address write */

/* Initial PSW for tasks: supervisor, GW=1, CDE=1, CDC=0x7F (unlimited).
 *
 * CDC=0x7F (all-ones) is the TriCore "unlimited nesting" sentinel: CALL and RET
 * do not decrement/increment the counter, so CDO/CDU never fire for task code.
 *
 * tricore_cpu_do_interrupt forces CDC=0, CDE=1 on every interrupt/trap entry.
 * cdc_zero(CDC=0) returns true in QEMU, so RFE on ISR exit does not raise NEST.
 *
 * Arch_StartFirstTask explicitly clears CDE before rslcx+rfe so that the
 * accumulated CDC from the boot call chain does not trigger NEST there. */
#define TRICORE_INITIAL_PSW \
    (TRICORE_PSW_IO_SUPERVISOR | TRICORE_PSW_GW | (1U << 7) | 0x7FU)

/* ---------------------------------------------------------------------------
 * ICR (Interrupt Control Register) bits
 * bits[7:0]  = CCPN (current CPU priority number, 0 = not in ISR)
 * bit[15]    = IE   (global interrupt enable)
 * bits[23:16]= PIPN (pending interrupt priority number)
 * --------------------------------------------------------------------------- */
#define TRICORE_ICR_IE              0x8000
#define TRICORE_ICR_CCPN_MASK       0xFF

/* ---------------------------------------------------------------------------
 * PCXI link word encoding — TC1.6.1 / TC1.6.2 (AURIX TC2xx/TC3xx)
 *
 * bits[15:0]  = PCXO (context offset, 64-byte units)
 * bits[19:16] = PCXS (context segment)
 * bit[20]     = UL   (0 = lower context, 1 = upper context)  ← TC1.6.1
 * bit[21]     = PIE  (previous interrupt enable)              ← TC1.6.1
 * bits[29:22] = PCPN (previous CPU priority number)
 *
 * Note: TC1.3 used UL at bit 22 and PIE at bit 23.  TC1.6.1 (tc2x, TC277D)
 * moved them to bits 20 and 21.  Wrong bit positions break context switching.
 * --------------------------------------------------------------------------- */
#define TRICORE_PCXI_UL             0x100000
#define TRICORE_PCXI_PIE            0x200000

/* ---------------------------------------------------------------------------
 * CSA address/link conversion macros
 * CSA must be 64-byte aligned (16 words × 4 bytes).
 *
 *  link → address:
 *    addr[31:28] = link[19:16] << 12
 *    addr[25:6]  = link[15:0]  << 6  (strips lower 6 bits = 64-byte alignment)
 *
 *  address → link:
 *    link[19:16] = addr[31:28] >> 12
 *    link[15:0]  = addr[25:6]  >> 6
 * --------------------------------------------------------------------------- */
#ifndef __ASSEMBLER__
#define TRICORE_CSA_TO_ADDR(link) \
    ((uint32_t *)(((((uint32_t)(link)) & 0x000F0000UL) << 12U) | \
                  ((((uint32_t)(link)) & 0x0000FFFFUL) << 6U)))

#define TRICORE_ADDR_TO_CSA(ptr) \
    ((uint32_t)(((((uint32_t)(ptr)) & 0xF0000000UL) >> 12U) | \
                ((((uint32_t)(ptr)) & 0x003FFFFCUL) >> 6U)))
#endif

/* ---------------------------------------------------------------------------
 * Upper CSA layout (16 words, offset in bytes from CSA base)
 * --------------------------------------------------------------------------- */
#define TRICORE_UCSA_PCXI    0     /* word[0]  previous PCXI link */
#define TRICORE_UCSA_PSW     1     /* word[1]  Program Status Word */
#define TRICORE_UCSA_A10     2     /* word[2]  stack pointer */
#define TRICORE_UCSA_A11     3     /* word[3]  return address / interrupted PC */
#define TRICORE_UCSA_D8      4
#define TRICORE_UCSA_D9      5
#define TRICORE_UCSA_D10     6
#define TRICORE_UCSA_D11     7
#define TRICORE_UCSA_A12     8
#define TRICORE_UCSA_A13     9
#define TRICORE_UCSA_A14     10
#define TRICORE_UCSA_A15     11
#define TRICORE_UCSA_D12     12
#define TRICORE_UCSA_D13     13
#define TRICORE_UCSA_D14     14
#define TRICORE_UCSA_D15     15

/* ---------------------------------------------------------------------------
 * Lower CSA layout (16 words)
 * --------------------------------------------------------------------------- */
#define TRICORE_LCSA_PCXI    0     /* word[0]  link to upper context (UL=1) */
#define TRICORE_LCSA_A11     1     /* word[1]  A11 = task return address / PC */
#define TRICORE_LCSA_A2      2
#define TRICORE_LCSA_A3      3
#define TRICORE_LCSA_D0      4
#define TRICORE_LCSA_D1      5
#define TRICORE_LCSA_D2      6
#define TRICORE_LCSA_D3      7
#define TRICORE_LCSA_A4      8     /* first argument register */
#define TRICORE_LCSA_A5      9
#define TRICORE_LCSA_D4      10
#define TRICORE_LCSA_D5      11
#define TRICORE_LCSA_A6      12
#define TRICORE_LCSA_A7      13
#define TRICORE_LCSA_D6      14
#define TRICORE_LCSA_D7      15

/* ---------------------------------------------------------------------------
 * TC27x (AURIX TC2xx) STM (System Timer Module) — for kernel tick
 *
 * On the linumiz QEMU TC277 machine, STM0 is mapped at 0xF0000000.
 * Real TC27xD hardware maps STM0 at 0xF0001000; adjust if targeting real HW.
 * --------------------------------------------------------------------------- */
#define TRICORE_STM0_BASE    0xF0000000UL
#define TRICORE_STM_TIM0     0x10U   /* 64-bit timer, lower 32-bit word */
#define TRICORE_STM_CMP0     0x30U   /* compare register 0 */
#define TRICORE_STM_CMCON    0x38U   /* compare control: MSIZE[4:0], MSTART[12:8] */
#define TRICORE_STM_ICR      0x3CU   /* interrupt ctrl: CMP0EN(0), CMP0IR(1) */
#define TRICORE_STM_ISCR     0x40U   /* interrupt set/clear: CMP0IRR (bit 0) */

#define TRICORE_STM_ICR_CMP0EN   0x1U  /* bit 0: enable CMP0 interrupt */
#define TRICORE_STM_ICR_CMP0IR   0x2U  /* bit 1: CMP0 interrupt flag */
#define TRICORE_STM_ISCR_CMP0IRR 0x1U  /* bit 0: clear CMP0IR */

/* CMCON: MSIZE0=31 (nbits=32), MSTART0=0 — compare full 32-bit TIM0 word */
#define TRICORE_STM_CMCON_VAL    0x001FU

/* ---------------------------------------------------------------------------
 * TC27x Service Request Nodes (SRN) — for enabling interrupts
 *
 * On the linumiz QEMU TC277 machine, the IR src_region is at 0xF0038000.
 * Each SRC entry is 4 bytes; SRC[N] is at base + N*4.
 *   STM0 SR0 (CMP0): QEMU SOC connects STM irq-0 to IR SRC input 0xC0
 *   CPU0 SR0 (soft):  free SRC slot at index 1 (no hardware device attached)
 *
 * SRC register format (TC3x mode, QEMU R_SRC_* fields):
 *   SRPN  [7:0]  = interrupt priority
 *   SRE   [10]   = service request enable (bit 10)
 *   SETR  [26]   = set request (software pend, clears itself)
 * --------------------------------------------------------------------------- */
#define TRICORE_SRC_BASE         0xF0038000UL
#define TRICORE_SRC_STM0_SR0     (TRICORE_SRC_BASE + 0x300U)  /* IR index 0xC0 */
#define TRICORE_SRC_CPU0_SR0     (TRICORE_SRC_BASE + 0x004U)  /* IR index 1 (software) */

#define TRICORE_SRN_EN           0x400U        /* SRE: bit 10 (TC3x mode) */
#define TRICORE_SRN_SETR         0x4000000U    /* SETR: bit 26 */
#define TRICORE_SRN_CLRR         0x2000000U    /* CLRR: bit 25 */
#define TRICORE_SRN_SRPN_MASK    0xFF           /* priority field */

/* Software IRQ priority — lower than tick, triggers softirq daemon */
#ifndef ARCH_TRICORE_SW_IRQ_PRIORITY
#define ARCH_TRICORE_SW_IRQ_PRIORITY    1
#endif

/* Tick interrupt priority */
#ifndef ARCH_TRICORE_TICK_PRIORITY
#define ARCH_TRICORE_TICK_PRIORITY      2
#endif
