#ifndef __ARCH_C2000_C28_DEFS_H
#define __ARCH_C2000_C28_DEFS_H

/*
 * C28x architecture port definitions — self-contained, no driverlib dependency.
 *
 * Reference: TMS320C28x CPU and Instruction Set Reference Manual (SPRU430)
 *            TMS320C28x Peripheral Reference Guide (SPRU566)
 *
 * Interrupt strategy:
 *   Tick:   CPU Timer 2 → CPU INT14 (direct, bypasses PIE)
 *   Yield:  RTOSINT     → forced via OR IFR, #0x2000
 *   SW IRQ: DLOGINT     → forced via OR IFR, #0x1000 (for softirq)
 *
 * PIE (Peripheral Interrupt Expansion) handles INT1-INT12.
 * CPU INT13 = CPU Timer 0, INT14 = CPU Timer 2 (both direct, no PIE ACK needed).
 */

/* -------------------------------------------------------------------------
 * CPU Timer 2 (RTOS tick clock)
 * Base address common across F28x family (byte-addressed on C28x = word*2).
 * C28x is 16-bit word addressed; peripheral addresses are 32-bit aligned.
 * -------------------------------------------------------------------------*/
#ifndef CONFIG_ARCH_CPUTIMER2_BASE
#define CONFIG_ARCH_CPUTIMER2_BASE          0x00000C00U
#endif

#define CPUTIMER_O_TIM      0x0U   /* Counter register (16-bit read) */
#define CPUTIMER_O_PRD      0x2U   /* Period register (16-bit words) */
#define CPUTIMER_O_TCR      0x4U   /* Control register */
#define CPUTIMER_O_TPR      0x6U   /* Prescale register low */
#define CPUTIMER_O_TPRH     0x7U   /* Prescale register high */

#define CPUTIMER_TCR_TSS    0x0010U  /* Timer stop */
#define CPUTIMER_TCR_TRB    0x0020U  /* Timer reload */
#define CPUTIMER_TCR_TIE    0x4000U  /* Interrupt enable */
#define CPUTIMER_TCR_TIF    0x8000U  /* Interrupt flag (write 1 to clear) */

/* -------------------------------------------------------------------------
 * PIE (Peripheral Interrupt Expansion) controller
 * Standard addresses across F28x family (word addresses, C28x 16-bit space).
 * -------------------------------------------------------------------------*/
#ifndef CONFIG_ARCH_PIECTRL_BASE
#define CONFIG_ARCH_PIECTRL_BASE            0x00000CE0U
#endif

/* PIE Control register (PIECTRL) */
#define PIECTRL_O_PIECTRL   0x0U   /* PIE control: bit 0 = ENPIE (enable fetch from vector table) */
#define PIECTRL_O_PIEACK    0x1U   /* PIE acknowledge (one bit per group 1-12) */

/* PIE Interrupt Enable Registers: PIEIER1..PIEIER12 (one per group) */
#define PIECTRL_O_IER(grp)  (0x2U + ((grp) - 1U) * 2U)
/* PIE Interrupt Flag Registers: PIEIFR1..PIEIFR12 */
#define PIECTRL_O_IFR(grp)  (0x3U + ((grp) - 1U) * 2U)

#define PIE_ACK_ALL         0x0FFFU  /* Acknowledge all 12 PIE groups */

/* -------------------------------------------------------------------------
 * CPU Interrupt Enable (IER) and Flag (IFR) registers
 * These are CPU-core registers accessed via assembly (no memory map).
 * IFR bit positions:
 *   Bit  0  = INT1  (PIE group 1)
 *   ...
 *   Bit 11  = INT12 (PIE group 12)
 *   Bit 12  = DLOGINT  → used as SW IRQ for softirq
 *   Bit 13  = RTOSINT  → used as yield/task-switch trigger
 * -------------------------------------------------------------------------*/
#define IFR_INT1_BIT        0x0001U
#define IFR_INT12_BIT       0x0800U
#define IFR_DLOGINT_BIT     0x1000U  /* DLOGINT: software IRQ for softirq */
#define IFR_RTOSINT_BIT     0x2000U  /* RTOSINT: voluntary yield trigger */

/* IER bits mirror IFR */
#define IER_INT14_BIT       0x0000U  /* INT14 not in IER; enabled via timer */
#define IER_RTOSINT_BIT     0x2000U
#define IER_DLOGINT_BIT     0x1000U

/* -------------------------------------------------------------------------
 * CPU vector table (PIE vector table when PIECTRL.ENPIE=1)
 * PIE vector table base: 0x000D00 (word address)
 * Layout: 8 words per group (32-bit vectors = 2 words each)
 * -------------------------------------------------------------------------*/
#define C28X_PIE_VECT_BASE      0x000D00U

/* Vector offset (word) for PIE group G, channel C (1-indexed) */
#define C28X_PIE_VECT_OFF(G, C) ((((G) - 1U) * 16U) + (((C) - 1U) * 2U))

/* CPU interrupt vectors (outside PIE, separate table entries) */
#define C28X_CPU_VECT_RESET     0x0000U
#define C28X_CPU_VECT_INT1      0x0002U  /* → PIE group 1 */
#define C28X_CPU_VECT_INT12     0x0018U  /* → PIE group 12 */
#define C28X_CPU_VECT_INT13     0x001AU  /* CPU Timer 0 direct */
#define C28X_CPU_VECT_INT14     0x001CU  /* CPU Timer 2 direct (RTOS tick) */
#define C28X_CPU_VECT_DLOGINT   0x001EU  /* DLOGINT (SW IRQ for softirq) */
#define C28X_CPU_VECT_RTOSINT   0x0020U  /* RTOSINT (yield) */

/* -------------------------------------------------------------------------
 * Irq_index mapping conventions
 *   0-11   : CPU interrupts INT1-INT12 (mapped via PIE groups)
 *   12     : DLOGINT (SW IRQ / softirq trigger)
 *   13     : RTOSINT (voluntary yield — not user-installable)
 *   14     : INT14 / CPU Timer 2 (tick — not user-installable)
 *   16-111 : PIE interrupts (group G, channel C) → index = 16 + (G-1)*8 + (C-1)
 * -------------------------------------------------------------------------*/
#define ARCH_C28_YIELD_IRQ_INDEX            13U  /* RTOSINT slot */
#define ARCH_C28_SW_IRQ_INDEX               12U  /* DLOGINT slot */
#define ARCH_C28_TICK_IRQ_INDEX             14U  /* INT14 slot */
#define ARCH_C28_PIE_BASE_INDEX             16U
#define ARCH_C28_PIE_INDEX(grp, ch)         (ARCH_C28_PIE_BASE_INDEX + ((grp) - 1U) * 8U + ((ch) - 1U))

/* -------------------------------------------------------------------------
 * Register access macros
 * C28x is 16-bit word addressed. Memory-mapped peripherals are at 16-bit
 * word addresses. Use __byte() / __word() compiler intrinsics or direct
 * pointer casts.
 * -------------------------------------------------------------------------*/
#define ARCH_HWREG16(addr)  (*((volatile uint16_t *)(addr)))
#define ARCH_HWREG32(addr)  (*((volatile uint32_t *)(addr)))

/* -------------------------------------------------------------------------
 * Stack and register layout constants
 * C28x stack grows upward. SP is XAR7.0 (16-bit word pointer).
 *
 * Registers NOT auto-saved by hardware on interrupt (must be saved manually):
 *   XAR0, XAR1 (lower 16-bit AR0/AR1 not saved; AR0H/AR1H are auto-saved)
 *   XAR2, XAR3, XAR4, XAR5, XAR6
 *   XT (full 32-bit; T upper 16 is auto-saved but XL lower is not)
 *   RPC (return program counter for LCR/RETL)
 *
 * Registers auto-saved by hardware (restored by IRET):
 *   T, ST0, AH, AL, PH, PL, AR0H, AR1H, DP, ST1, DBGSTAT, IER, PC
 *
 * FPU registers (if CONFIG_HAS_FLOAT > 0):
 *   R0H-R7H, STF, RB
 * -------------------------------------------------------------------------*/

/* Number of 16-bit words pushed in SAVE_CONTEXT (manually saved part):
 *   XAR0-XAR6 = 7 × 2 words = 14 words
 *   XT         =     2 words
 *   RPC        =     2 words
 *   Total basic = 18 words
 *   FPU: R0H-R7H = 8×2=16 words, STF=2, RB=1 → +19 words
 */
#if CONFIG_HAS_FLOAT > 0
#define ARCH_C28_CONTEXT_WORDS  37U
#else
#define ARCH_C28_CONTEXT_WORDS  18U
#endif

/* Initial ST0 / ST1 values for a new task (OVM=0, SXM=1, C=0, etc.) */
#define ARCH_C28_INIT_ST0       0x0000U  /* PM=0, no overflow, C28 mode */
#define ARCH_C28_INIT_ST1       0x8A08U  /* EALLOW=0, C28AMODE=1, VMAP=1, SPA=1 */
#define ARCH_C28_INIT_IER       0xA000U  /* Enable INT14 (bit13) and RTOSINT (bit13)... wait */

/* IER initial value: enable INT14 (bit13=RTOSINT, bit14=... actually INT14 is not in IER)
 * On C28x, INT14 (CPU Timer 2) enable/disable is controlled by the timer's TIE bit + EINT.
 * We only need RTOSINT and DLOGINT in IER for yield and SW IRQ.
 */
#define ARCH_C28_INIT_IER_VALUE (IER_RTOSINT_BIT | IER_DLOGINT_BIT)

/* -------------------------------------------------------------------------
 * Misc
 * -------------------------------------------------------------------------*/
#define ARCH_DISABLE_INTERRUPTS()  __asm(" DINT")
#define ARCH_ENABLE_INTERRUPTS()   __asm(" EINT")

extern void ArchTickIsr(void);
extern void ArchSwitchIsr(void);
extern void ArchSwIrqIsr(void);

#endif /* __ARCH_C2000_C28_DEFS_H */
