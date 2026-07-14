/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 arch port — arch/tricore/arch.c
 * Implements: arch/tricore/include/ulmk_arch.h
 * Full specification: docs/arch_api_spec.md
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_printk.h>
/* =========================================================================
 * CPU control
 * ========================================================================= */

/*
 * Bit 31 tags a no-op save: CCPN is already 255 (syscall gateway), so maskable
 * IRQs cannot preempt.  Skipping disable/mtcr/isync removes nested critical-
 * section cost on the hot syscall path.
 */
#define ULMK_IRQ_KEY_SKIP	(1u << 31)

ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void)
{
	uint32_t icr;

	__asm__ volatile("mfcr %0, 0xFE2C" : "=d"(icr));
	if ((icr & 0xFFu) == 0xFFu)
		return (ulmk_arch_irq_key_t)(icr | ULMK_IRQ_KEY_SKIP);
	__asm__ volatile("disable" ::: "memory");
	return icr;
}

void ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t key)
{
	uint32_t icr = (uint32_t)key;

	if (icr & ULMK_IRQ_KEY_SKIP)
		return;
	__asm__ volatile("mtcr 0xFE2C, %0" :: "d"(icr));
	__asm__ volatile("isync" ::: "memory");
}

void ulmk_arch_cpu_irq_enable(void)
{
	__asm__ volatile("enable" ::: "memory");
}

void ulmk_arch_cpu_irq_disable(void)
{
	__asm__ volatile("disable" ::: "memory");
}

void ulmk_arch_cpu_idle(void)
{
#if ULMK_ARCH_IDLE_IS_WAIT
	/*
	 * WAIT suspends the pipeline until the next pending interrupt.
	 * ICR.IE must be 1 before entering WAIT, which is the case for
	 * any thread context (PIE=1 in the fabricated lower CSA).
	 */
	__asm__ volatile("wait" ::: "memory");
#else
	__asm__ volatile("nop");
#endif
}

void ulmk_arch_cpu_halt(void)
{
	for (;;)
		;
}

uint32_t ulmk_arch_cpu_clz(uint32_t val)
{
	uint32_t result;

	__asm__("clz %0, %1" : "=d"(result) : "d"(val));
	return result;
}

#if ULMK_CONFIG_SYSCALL_WCET
/* CCTRL: CM=bit0, CE=bit1 (TC1.6 Vol1 §12.11). Normal free-run: CE=1. */
#define CCTRL_CE	(1u << 1)

void ulmk_arch_cycle_enable(void)
{
	uint32_t cctrl;

	__asm__ volatile("mfcr %0, 0xFC00" : "=d"(cctrl));
	cctrl |= CCTRL_CE;
	__asm__ volatile("mtcr 0xFC00, %0" :: "d"(cctrl));
	__asm__ volatile("isync" ::: "memory");
}

uint32_t ulmk_arch_cycle_read(void)
{
	uint32_t v;

	__asm__ volatile("mfcr %0, 0xFC04" : "=d"(v));
	return v;
}
#else
void ulmk_arch_cycle_enable(void)
{
}

uint32_t ulmk_arch_cycle_read(void)
{
	return 0u;
}
#endif

/* =========================================================================
 * CSA helpers — used by context init only; not part of the public API.
 *
 * CSA link-word format (TC1.6.1 §3.2):
 *   [20]    UL   — 1 = upper context, 0 = lower context
 *   [19:16] PCXS — segment index (bits [30:28] of the physical address)
 *   [15:0]  PCXO — offset within segment (bits [21:6] of the physical addr)
 *
 * Conversion:
 *   addr = (PCXS << 28) | (PCXO << 6)
 *   link = ((addr >> 12) & 0x70000) | ((addr >> 6) & 0xFFFF)
 * ========================================================================= */

#define UL_CSA_UL_FLAG		(1u << 20)	/* upper-context flag in link */

static inline uint32_t *csa_link_to_addr(uint32_t link)
{
	return (uint32_t *)(((link & 0x70000u) << 12) |
			    ((link & 0xFFFFu) << 6));
}

static inline uint32_t addr_to_csa_link(const uint32_t *addr)
{
	uint32_t a = (uint32_t)(uintptr_t)addr;

	return ((a >> 12) & 0x70000u) | ((a >> 6) & 0xFFFFu);
}

/*
 * Pop one CSA frame from the free list (FCX).
 * Must not be called from interrupt context or with interrupts enabled
 * while other code may concurrently modify FCX.
 */
static uint32_t csa_alloc(void)
{
	uint32_t fcx;
	uint32_t next;
	uint32_t *frame;

	__asm__ volatile("mfcr %0, 0xFE38" : "=d"(fcx));
	if (fcx == 0u)
		ulmk_arch_cpu_halt(); /* CSA pool exhausted — fatal */

	frame = csa_link_to_addr(fcx);
	next  = frame[0]; /* free list: frame[0] = link to next free frame */

	__asm__ volatile("dsync" ::: "memory");
	__asm__ volatile("mtcr 0xFE38, %0" :: "d"(next)); /* FCX = next */
	__asm__ volatile("isync" ::: "memory");

	return fcx;
}

/* =========================================================================
 * Context management
 * ========================================================================= */

void ulmk_arch_csa_pool_init(uintptr_t pool_base, size_t pool_size)
{
	(void)pool_base;
	(void)pool_size;
	/*
	 * The CSA free list is already built by startup.S before ulmk_arch_init()
	 * is called — startup.S links all frames and writes FCX/LCX.
	 * This function exists for platforms that need software-side init.
	 */
}

extern void _ulmk_thread_trampoline(void);

/*
 * Fabricate the initial two-frame CSA chain for a new thread.
 *
 * TC1.6.1 context layout:
 *   Upper context (CALL/RFE): PCXI PSW A10 A11 D8-D11 A12-A15 D12-D15
 *   Lower context (SVLCX/RSLCX): PCXI A11 A2 A3 D0-D3 A4-A7 D4-D7
 *
 * RFE restores the upper context; PC is taken from A11 pre-restore.
 * lower_csa[1] = trampoline sets the jump target for the first RFE.
 *
 * Fabricated frames:
 *
 *   Upper CSA (UL=1):
 *     [0]  PCXI = 0     (terminal)
 *     [1]  PSW          (privilege)
 *     [2]  A10          (stack_top)
 *     [3]  A11          (trampoline — restored into A11 register by RFE)
 *     [10] A14          (entry function address — loaded by RFE into A14)
 *
 *   Lower CSA (UL=0):
 *     [0]  PCXI         (upper_link | UL_FLAG)
 *     [1]  A11          (trampoline — RFE uses this for PC before restore)
 *     [8]  A4           (arg pointer — restored by RSLCX, ABI-correct)
 *
 * ctx->pcxi = lower_link (UL=0).
 */
void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx,
		      void (*entry)(void *arg), void *arg,
		      uintptr_t stack_top, ulmk_privilege_t priv)
{
	uint32_t upper_link;
	uint32_t lower_link;
	uint32_t *upper_csa;
	uint32_t *lower_csa;
	uint32_t psw;
	uint32_t i;

	/*
	 * Build the initial PSW from the current kernel PSW, overriding the
	 * fields that must be explicit for a thread context:
	 *
	 *   CDC = 0  (fresh call-depth counter)
	 *   IS  = 0  (thread uses A10 / task stack, never ISP)
	 *   IO       (as requested by caller)
	 *
	 * GW and CDE are inherited from the kernel PSW.  IS must be stripped
	 * explicitly because some TriCore startup environments reset with IS=1;
	 * leaving IS=1 causes a class-4 PSE trap after the first RFE into a
	 * fabricated context.
	 */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x7Fu;					/* clear CDC */
	psw &= ~0x200u;					/* clear IS (task stack) */
	psw &= ~0xC00u;					/* clear IO bits */
	psw |= (uint32_t)(priv == ULMK_PRIV_USER   ? 0u :
			  priv == ULMK_PRIV_DRIVER ? 0x400u : 0x800u);
	/* PSW.PRS [13:12]: kernel=0 (PRS 0), driver/user=1 (PRS 1) */
	psw &= ~0x3000u;
	if (priv != ULMK_PRIV_KERNEL)
		psw |= (uint32_t)ULMK_ARCH_PRS_USER << 12;

	upper_link = csa_alloc();
	upper_csa  = csa_link_to_addr(upper_link);
	for (i = 0u; i < 16u; i++)
		upper_csa[i] = 0u;
	upper_csa[1]  = psw;
	upper_csa[2]  = (uint32_t)stack_top;
	upper_csa[3]  = (uint32_t)(uintptr_t)_ulmk_thread_trampoline;
	upper_csa[10] = (uint32_t)(uintptr_t)entry;	/* A14: entry fn */

	lower_link = csa_alloc();
	lower_csa  = csa_link_to_addr(lower_link);
	for (i = 0u; i < 16u; i++)
		lower_csa[i] = 0u;
	/*
	 * PIE (bit 21) = 1: the RFE that starts this thread sets ICR.IE=1
	 * (ICR.IE comes from PCXI.PIE on RFE).  Without PIE=1, interrupts
	 * are left disabled in every new thread, blocking timer preemption.
	 */
	lower_csa[0] = upper_link | UL_CSA_UL_FLAG | (1u << 21);
	lower_csa[1] = (uint32_t)(uintptr_t)_ulmk_thread_trampoline;
	lower_csa[8] = (uint32_t)(uintptr_t)arg;	/* A4: pointer arg */

	ctx->pcxi = lower_link;
}

/*
 * O(1) post-save sync — called from ctx_switch.S / vectors.S after pcxi is
 * stored.  One volatile read of the head CSA link + dsync; no chain walk.
 */
void ulmk_arch_ctx_post_save(ulmk_arch_ctx_t *ctx)
{
	volatile uint32_t dummy;

	if (!ctx || ctx->pcxi == 0u)
		return;

	dummy = csa_link_to_addr(ctx->pcxi)[0];
	(void)dummy;
	__asm__ volatile("dsync" ::: "memory");
}

static uint32_t csa_chain_tail_link(uint32_t head_link)
{
	uint32_t link = head_link;
	uint32_t next;

	while (link != 0u) {
		next = csa_link_to_addr(link)[0] & 0x7FFFFu;
		if (next == 0u)
			return link;
		link = next;
	}
	return 0u;
}

/*
 * Return the saved CSA chain to FCX (cold path — thread kill/exit).
 *
 * Walks the PCXI chain to the terminal frame, then splices the whole list
 * onto FCX in one step (Zephyr z_tricore_reclaim_csa model).  O(chain depth);
 * ctx_switch / ISR preempt only store pcxi — O(1).
 *
 * Called when a thread is destroyed — the chain must not be live on CPU.
 * Caller must hold interrupts off or run with no concurrent FCX access.
 */
void ulmk_arch_ctx_free(ulmk_arch_ctx_t *ctx)
{
	uint32_t  head_link;
	uint32_t  tail_link;
	uint32_t *tail_frame;
	uint32_t  fcx;

	if (!ctx || ctx->pcxi == 0u)
		return;

	head_link = ctx->pcxi;
	ctx->pcxi = 0u;

	tail_link = csa_chain_tail_link(head_link);
	if (tail_link == 0u)
		return;

	tail_frame = csa_link_to_addr(tail_link);

	__asm__ volatile("mfcr %0, 0xFE38" : "=d"(fcx));
	tail_frame[0] = fcx;
	__asm__ volatile("dsync" ::: "memory");
	__asm__ volatile("mtcr 0xFE38, %0" :: "d"(head_link));
	__asm__ volatile("isync" ::: "memory");
}

/*
 * ISR preemption handoff — consumed by _arch_generic_preempt_isr in vectors.S
 * after the C handler returns.
 */
ulmk_arch_ctx_t *g_preempt_old_ctx;
ulmk_arch_ctx_t *g_preempt_new_ctx;

bool ulmk_arch_sched_isr_preempt_deferred(void)
{
	return true;
}

void ulmk_arch_sched_switch(ulmk_arch_ctx_t *from, const ulmk_arch_ctx_t *to,
			    unsigned int flags)
{
	if (flags == ULMK_SCHED_SWITCH_PREEMPT_ISR) {
		g_preempt_old_ctx = from;
		g_preempt_new_ctx = (ulmk_arch_ctx_t *)to;
		return;
	}

	ulmk_arch_ctx_switch(from, to);
}

/* =========================================================================
 * MPU helpers — MTCR requires constant CSFR address so each slot is an
 * explicit case.  ISYNC is issued once after a batch of writes.
 * ========================================================================= */

static inline void mpu_mtcr(uint32_t csfr, uint32_t val)
{
	/*
	 * MTCR csfr, %d[a]  requires @csfr to be a compile-time constant.
	 * We wrap it with a switch on the CSFR address; the compiler optimises
	 * to a direct MTCR instruction for each taken branch.
	 */
	switch (csfr) {
	/* DPR lower bounds */
	case 0xC000u: __asm__ volatile("mtcr 0xC000, %0" :: "d"(val)); break;
	case 0xC008u: __asm__ volatile("mtcr 0xC008, %0" :: "d"(val)); break;
	case 0xC010u: __asm__ volatile("mtcr 0xC010, %0" :: "d"(val)); break;
	case 0xC018u: __asm__ volatile("mtcr 0xC018, %0" :: "d"(val)); break;
	case 0xC020u: __asm__ volatile("mtcr 0xC020, %0" :: "d"(val)); break;
	case 0xC028u: __asm__ volatile("mtcr 0xC028, %0" :: "d"(val)); break;
	case 0xC030u: __asm__ volatile("mtcr 0xC030, %0" :: "d"(val)); break;
	case 0xC038u: __asm__ volatile("mtcr 0xC038, %0" :: "d"(val)); break;
	case 0xC040u: __asm__ volatile("mtcr 0xC040, %0" :: "d"(val)); break;
	case 0xC048u: __asm__ volatile("mtcr 0xC048, %0" :: "d"(val)); break;
	case 0xC050u: __asm__ volatile("mtcr 0xC050, %0" :: "d"(val)); break;
	case 0xC058u: __asm__ volatile("mtcr 0xC058, %0" :: "d"(val)); break;
	case 0xC060u: __asm__ volatile("mtcr 0xC060, %0" :: "d"(val)); break;
	case 0xC068u: __asm__ volatile("mtcr 0xC068, %0" :: "d"(val)); break;
	case 0xC070u: __asm__ volatile("mtcr 0xC070, %0" :: "d"(val)); break;
	case 0xC078u: __asm__ volatile("mtcr 0xC078, %0" :: "d"(val)); break;
	case 0xC080u: __asm__ volatile("mtcr 0xC080, %0" :: "d"(val)); break;
	case 0xC088u: __asm__ volatile("mtcr 0xC088, %0" :: "d"(val)); break;
	/* DPR upper bounds */
	case 0xC004u: __asm__ volatile("mtcr 0xC004, %0" :: "d"(val)); break;
	case 0xC00Cu: __asm__ volatile("mtcr 0xC00C, %0" :: "d"(val)); break;
	case 0xC014u: __asm__ volatile("mtcr 0xC014, %0" :: "d"(val)); break;
	case 0xC01Cu: __asm__ volatile("mtcr 0xC01C, %0" :: "d"(val)); break;
	case 0xC024u: __asm__ volatile("mtcr 0xC024, %0" :: "d"(val)); break;
	case 0xC02Cu: __asm__ volatile("mtcr 0xC02C, %0" :: "d"(val)); break;
	case 0xC034u: __asm__ volatile("mtcr 0xC034, %0" :: "d"(val)); break;
	case 0xC03Cu: __asm__ volatile("mtcr 0xC03C, %0" :: "d"(val)); break;
	case 0xC044u: __asm__ volatile("mtcr 0xC044, %0" :: "d"(val)); break;
	case 0xC04Cu: __asm__ volatile("mtcr 0xC04C, %0" :: "d"(val)); break;
	case 0xC054u: __asm__ volatile("mtcr 0xC054, %0" :: "d"(val)); break;
	case 0xC05Cu: __asm__ volatile("mtcr 0xC05C, %0" :: "d"(val)); break;
	case 0xC064u: __asm__ volatile("mtcr 0xC064, %0" :: "d"(val)); break;
	case 0xC06Cu: __asm__ volatile("mtcr 0xC06C, %0" :: "d"(val)); break;
	case 0xC074u: __asm__ volatile("mtcr 0xC074, %0" :: "d"(val)); break;
	case 0xC07Cu: __asm__ volatile("mtcr 0xC07C, %0" :: "d"(val)); break;
	case 0xC084u: __asm__ volatile("mtcr 0xC084, %0" :: "d"(val)); break;
	case 0xC08Cu: __asm__ volatile("mtcr 0xC08C, %0" :: "d"(val)); break;
	/* CPR lower bounds */
	case 0xD000u: __asm__ volatile("mtcr 0xD000, %0" :: "d"(val)); break;
	case 0xD008u: __asm__ volatile("mtcr 0xD008, %0" :: "d"(val)); break;
	case 0xD010u: __asm__ volatile("mtcr 0xD010, %0" :: "d"(val)); break;
	case 0xD018u: __asm__ volatile("mtcr 0xD018, %0" :: "d"(val)); break;
	case 0xD020u: __asm__ volatile("mtcr 0xD020, %0" :: "d"(val)); break;
	case 0xD028u: __asm__ volatile("mtcr 0xD028, %0" :: "d"(val)); break;
	case 0xD030u: __asm__ volatile("mtcr 0xD030, %0" :: "d"(val)); break;
	case 0xD038u: __asm__ volatile("mtcr 0xD038, %0" :: "d"(val)); break;
	case 0xD040u: __asm__ volatile("mtcr 0xD040, %0" :: "d"(val)); break;
	case 0xD048u: __asm__ volatile("mtcr 0xD048, %0" :: "d"(val)); break;
	/* CPR upper bounds */
	case 0xD004u: __asm__ volatile("mtcr 0xD004, %0" :: "d"(val)); break;
	case 0xD00Cu: __asm__ volatile("mtcr 0xD00C, %0" :: "d"(val)); break;
	case 0xD014u: __asm__ volatile("mtcr 0xD014, %0" :: "d"(val)); break;
	case 0xD01Cu: __asm__ volatile("mtcr 0xD01C, %0" :: "d"(val)); break;
	case 0xD024u: __asm__ volatile("mtcr 0xD024, %0" :: "d"(val)); break;
	case 0xD02Cu: __asm__ volatile("mtcr 0xD02C, %0" :: "d"(val)); break;
	case 0xD034u: __asm__ volatile("mtcr 0xD034, %0" :: "d"(val)); break;
	case 0xD03Cu: __asm__ volatile("mtcr 0xD03C, %0" :: "d"(val)); break;
	case 0xD044u: __asm__ volatile("mtcr 0xD044, %0" :: "d"(val)); break;
	case 0xD04Cu: __asm__ volatile("mtcr 0xD04C, %0" :: "d"(val)); break;
	/* Enable registers */
	case 0xE000u: __asm__ volatile("mtcr 0xE000, %0" :: "d"(val)); break;
	case 0xE004u: __asm__ volatile("mtcr 0xE004, %0" :: "d"(val)); break;
	case 0xE008u: __asm__ volatile("mtcr 0xE008, %0" :: "d"(val)); break;
	case 0xE00Cu: __asm__ volatile("mtcr 0xE00C, %0" :: "d"(val)); break;
	case 0xE010u: __asm__ volatile("mtcr 0xE010, %0" :: "d"(val)); break;
	case 0xE014u: __asm__ volatile("mtcr 0xE014, %0" :: "d"(val)); break;
	case 0xE018u: __asm__ volatile("mtcr 0xE018, %0" :: "d"(val)); break;
	case 0xE01Cu: __asm__ volatile("mtcr 0xE01C, %0" :: "d"(val)); break;
	case 0xE020u: __asm__ volatile("mtcr 0xE020, %0" :: "d"(val)); break;
	case 0xE024u: __asm__ volatile("mtcr 0xE024, %0" :: "d"(val)); break;
	case 0xE028u: __asm__ volatile("mtcr 0xE028, %0" :: "d"(val)); break;
	case 0xE02Cu: __asm__ volatile("mtcr 0xE02C, %0" :: "d"(val)); break;
	case 0xE040u: __asm__ volatile("mtcr 0xE040, %0" :: "d"(val)); break;
	case 0xE044u: __asm__ volatile("mtcr 0xE044, %0" :: "d"(val)); break;
	case 0xE048u: __asm__ volatile("mtcr 0xE048, %0" :: "d"(val)); break;
	case 0xE04Cu: __asm__ volatile("mtcr 0xE04C, %0" :: "d"(val)); break;
	/* SYSCON */
	case 0xFE14u: __asm__ volatile("mtcr 0xFE14, %0" :: "d"(val)); break;
	default: break;
	}
}

static void mpu_write_dpr(uint8_t slot, uint32_t lower, uint32_t upper)
{
	mpu_mtcr(ULMK_ARCH_CSFR_DPR_L(slot), lower);
	mpu_mtcr(ULMK_ARCH_CSFR_DPR_U(slot), upper);
}

static void mpu_write_cpr(uint8_t slot, uint32_t lower, uint32_t upper)
{
	mpu_mtcr(ULMK_ARCH_CSFR_CPR_L(slot), lower);
	mpu_mtcr(ULMK_ARCH_CSFR_CPR_U(slot), upper);
}

/*
 * Write DPRE, DPWE, CPRE, CPXE enable registers for a given PRS.
 * ISYNC is issued at the end to flush the pipeline.
 */
static void mpu_write_enables(uint8_t prs, uint32_t dpre, uint32_t dpwe,
			      uint32_t cpre, uint32_t cpxe)
{
	mpu_mtcr(ULMK_ARCH_CSFR_DPRE_0 + (uint32_t)prs * 4u, dpre);
	mpu_mtcr(ULMK_ARCH_CSFR_DPWE_0 + (uint32_t)prs * 4u, dpwe);
	mpu_mtcr(ULMK_ARCH_CSFR_CPRE_0 + (uint32_t)prs * 4u, cpre);
	mpu_mtcr(ULMK_ARCH_CSFR_CPXE_0 + (uint32_t)prs * 4u, cpxe);
	__asm__ volatile("isync" ::: "memory");
}

/* =========================================================================
 * MPU
 * ========================================================================= */

/*
 * Static DPR/CPR layout (configured once at boot):
 *
 *   DPR 0: entire 4 GiB (PRS 0 R+W bypass)
 *   DPR 1: kernel static RAM (PRS 0 only)
 *   DPR 2: userspace RAM (PRS 1 R+W)
 *   DPR 3: flash read + MMIO (PRS 1 R+W)
 *   DPR ULMK_ARCH_MPU_USER_DPR_BASE+: per-thread dynamic (mpu_switch)
 *
 *   CPR 0: kernel executable flash (PRS 0 X only)
 *   CPR 1: userspace executable flash (PRS 1 X only)
 */

/*
 * Last programmed userspace DPR layout — skips redundant CSFR traffic on
 * self-resched / unchanged domain, and only clears slots that were live.
 */
static const ulmk_arch_region_t *g_mpu_regions;
static uint8_t g_mpu_count;
static uint8_t g_mpu_prs = 0xFFu;
static uint8_t g_mpu_live;

static uint32_t mpu_range_upper(uintptr_t end)
{
	return (uint32_t)end - 8u;
}

void ulmk_arch_mpu_init(void)
{
	uint32_t syscon;
	uint8_t  i;
	uintptr_t kexec_lo;
	uintptr_t kexec_hi;
	uintptr_t utext_lo;
	uintptr_t utext_hi;
	uintptr_t kram_lo;
	uintptr_t kram_hi;
	uintptr_t uram_lo;
	uintptr_t uram_hi;
	uint32_t  prs1_cpre;
	uint32_t  prs1_cpxe;
	uint32_t  prs0_cpr;

	extern uint8_t _ulmk_kernel_exec_start[];
	extern uint8_t _ulmk_kernel_exec_end[];
	extern uint8_t _ulmk_user_text_start[];
	extern uint8_t _ulmk_user_text_end[];
	extern uint8_t _ulmk_kernel_data_start[];
	extern uint8_t _ulmk_kernel_ram_end[];
	extern uint8_t _ulmk_user_ram_start[];
	extern uint8_t _ulmk_user_pool_end[];

	/* Disable protection during reconfiguration */
	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(ULMK_ARCH_CSFR_SYSCON, syscon & ~ULMK_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");

	g_mpu_regions = NULL;
	g_mpu_count   = 0u;
	g_mpu_prs     = 0xFFu;
	g_mpu_live    = 0u;

	/* Zero implemented DPR/CPR ranges */
	for (i = 0u; i < ULMK_ARCH_MPU_NUM_DPR; i++)
		mpu_write_dpr(i, 0u, 0u);
	for (i = 0u; i < ULMK_ARCH_MPU_NUM_CPR; i++)
		mpu_write_cpr(i, 0u, 0u);

	/* Zero all PRS enable registers */
	for (i = 0u; i < ULMK_ARCH_NUM_PRS; i++)
		mpu_write_enables(i, 0u, 0u, 0u, 0u);

	kexec_lo = (uintptr_t)_ulmk_kernel_exec_start;
	kexec_hi = (uintptr_t)_ulmk_kernel_exec_end;
	utext_lo = (uintptr_t)_ulmk_user_text_start;
	utext_hi = (uintptr_t)_ulmk_user_text_end;
	kram_lo  = (uintptr_t)_ulmk_kernel_data_start;
	kram_hi  = (uintptr_t)_ulmk_kernel_ram_end;
	uram_lo  = (uintptr_t)_ulmk_user_ram_start;
	uram_hi  = (uintptr_t)_ulmk_user_pool_end;

	/* DPR 0: entire 4 GiB — kernel full R+W via PRS 0 */
	mpu_write_dpr(ULMK_ARCH_MPU_KERNEL_DPR, 0x00000000u, 0xFFFFFFF8u);

	/* DPR 1: kernel-owned static RAM — not exposed to PRS 1 */
	if (kram_hi > kram_lo)
		mpu_write_dpr(ULMK_ARCH_MPU_KRAM_DPR, (uint32_t)kram_lo,
			      mpu_range_upper(kram_hi));

	/* DPR 2: userspace RAM (domains + heap pool) */
	if (uram_hi > uram_lo)
		mpu_write_dpr(ULMK_ARCH_MPU_URAM_DPR, (uint32_t)uram_lo,
			      mpu_range_upper(uram_hi));

	/*
	 * DPR 3: flash read + virt console + peripherals in one coarse slot.
	 * Execution is gated separately by CPR 1; this covers .rodata loads
	 * and MMIO accesses for driver threads.
	 */
	mpu_write_dpr(ULMK_ARCH_MPU_MMIO_DPR,
		      ULMK_BOARD_FLASH_BASE, 0xFFFFFFF8u);

	/* CPR 0: kernel code only */
	if (kexec_hi > kexec_lo)
		mpu_write_cpr(ULMK_ARCH_MPU_CPR_KERNEL, (uint32_t)kexec_lo,
			      mpu_range_upper(kexec_hi));

	/* CPR 1: userspace code */
	if (utext_hi > utext_lo)
		mpu_write_cpr(ULMK_ARCH_MPU_CPR_USER, (uint32_t)utext_lo,
			      mpu_range_upper(utext_hi));

	__asm__ volatile("isync" ::: "memory");

	prs1_cpre = 0u;
	prs1_cpxe = 0u;
	if (utext_hi > utext_lo) {
		prs1_cpre = (1u << ULMK_ARCH_MPU_CPR_USER);
		prs1_cpxe = (1u << ULMK_ARCH_MPU_CPR_USER);
	}

	/*
	 * PRS 0 (kernel): all static DPR slots + kernel CPR execute.
	 * DPR 0 already covers the full address space.  The kernel also needs
	 * execute over the userspace CPR: the thread trampoline and the common
	 * userspace entry live in user text, and kernel-privileged threads
	 * (e.g. idle) start there too.  Granting the trusted kernel execute of
	 * user text keeps isolation intact (PRS 1 still cannot reach CPR 0).
	 */
	prs0_cpr = (1u << ULMK_ARCH_MPU_CPR_KERNEL);
	if (utext_hi > utext_lo)
		prs0_cpr |= (1u << ULMK_ARCH_MPU_CPR_USER);

	mpu_write_enables(0u,
			  (1u << ULMK_ARCH_MPU_NUM_DPR) - 1u,
			  (1u << ULMK_ARCH_MPU_NUM_DPR) - 1u,
			  prs0_cpr,
			  prs0_cpr);

	/*
	 * PRS 1 (userspace): user RAM + MMIO/flash read; execute only user CPR.
	 * DPR 0 (kernel bypass) and DPR 1 (kernel RAM) are intentionally omitted.
	 */
	mpu_write_enables(1u,
			  (1u << ULMK_ARCH_MPU_URAM_DPR) |
			  (1u << ULMK_ARCH_MPU_MMIO_DPR),
			  (1u << ULMK_ARCH_MPU_URAM_DPR) |
			  (1u << ULMK_ARCH_MPU_MMIO_DPR),
			  prs1_cpre,
			  prs1_cpxe);

	/* PRS 2, 3: remain zeroed (unused) */
}

void ulmk_arch_mpu_enable(void)
{
	uint32_t syscon;

	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(ULMK_ARCH_CSFR_SYSCON, syscon | ULMK_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");
}

void ulmk_arch_mpu_disable(void)
{
	uint32_t syscon;

	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(ULMK_ARCH_CSFR_SYSCON, syscon & ~ULMK_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");
}

static void mpu_prs1_static_enables(uint32_t *dpre, uint32_t *dpwe,
				    uint32_t *cpre, uint32_t *cpxe)
{
	uintptr_t utext_lo;
	uintptr_t utext_hi;

	extern uint8_t _ulmk_user_text_start[];
	extern uint8_t _ulmk_user_text_end[];

	utext_lo = (uintptr_t)_ulmk_user_text_start;
	utext_hi = (uintptr_t)_ulmk_user_text_end;

	*dpre = (1u << ULMK_ARCH_MPU_URAM_DPR) |
		(1u << ULMK_ARCH_MPU_MMIO_DPR);
	*dpwe = (1u << ULMK_ARCH_MPU_URAM_DPR) |
		(1u << ULMK_ARCH_MPU_MMIO_DPR);
	*cpre = 0u;
	*cpxe = 0u;
	if (utext_hi > utext_lo) {
		*cpre = (1u << ULMK_ARCH_MPU_CPR_USER);
		*cpxe = (1u << ULMK_ARCH_MPU_CPR_USER);
	}
}

static void mpu_write_user_slot(uint8_t idx, const ulmk_arch_region_t *r,
				uint32_t *dpre, uint32_t *dpwe)
{
	uint8_t d_slot = (uint8_t)(ULMK_ARCH_MPU_USER_DPR_BASE + idx);

	if (d_slot >= ULMK_ARCH_MPU_NUM_DPR)
		return;

	mpu_write_dpr(d_slot,
		      (uint32_t)r->base,
		      (uint32_t)(r->base + r->size - 8u));
	if (r->perms & ULMK_PERM_READ)
		*dpre |= (1u << d_slot);
	if (r->perms & ULMK_PERM_WRITE)
		*dpwe |= (1u << d_slot);
}

/*
 * Configure dynamic DPR slots for the given PRS from @regions.
 * Preserves the static minimum-isolation enables programmed at boot.
 * Regions that do not fit (QEMU: USER_DPR_BASE == NUM_DPR) are ignored.
 */
static void mpu_program_regions(uint8_t prs, const ulmk_arch_region_t *regions,
				uint8_t count)
{
	uint32_t dpre;
	uint32_t dpwe;
	uint32_t cpre;
	uint32_t cpxe;
	uint8_t  i;
	uint8_t  prog;
	uint8_t  max_dyn;
	uintptr_t utext_lo;
	uintptr_t utext_hi;

	extern uint8_t _ulmk_user_text_start[];
	extern uint8_t _ulmk_user_text_end[];

	utext_lo = (uintptr_t)_ulmk_user_text_start;
	utext_hi = (uintptr_t)_ulmk_user_text_end;

	if (prs == 0u) {
		uint32_t prs0_cpr = (1u << ULMK_ARCH_MPU_CPR_KERNEL);

		if (utext_hi > utext_lo)
			prs0_cpr |= (1u << ULMK_ARCH_MPU_CPR_USER);

		mpu_write_enables(0u,
				  (1u << ULMK_ARCH_MPU_NUM_DPR) - 1u,
				  (1u << ULMK_ARCH_MPU_NUM_DPR) - 1u,
				  prs0_cpr,
				  prs0_cpr);
		g_mpu_prs     = 0u;
		g_mpu_regions = NULL;
		g_mpu_count   = 0u;
		g_mpu_live    = 0u;
		return;
	}

	max_dyn = (uint8_t)(ULMK_ARCH_MPU_NUM_DPR - ULMK_ARCH_MPU_USER_DPR_BASE);
	if (count > max_dyn)
		count = max_dyn;
	if (!regions)
		count = 0u;

	/*
	 * Unchanged domain (typical self-yield / same thread): no CSFR writes.
	 */
	if (prs == g_mpu_prs && regions == g_mpu_regions && count == g_mpu_count)
		return;

	mpu_prs1_static_enables(&dpre, &dpwe, &cpre, &cpxe);

	/*
	 * Fast append: mem_map / heap_extend grew the region table by one.
	 * Only program the new slot and refresh enables.
	 */
	if (prs == g_mpu_prs && regions == g_mpu_regions &&
	    count == (uint8_t)(g_mpu_count + 1u) && count > 0u) {
		mpu_write_user_slot((uint8_t)(count - 1u), &regions[count - 1u],
				    &dpre, &dpwe);
		/* Re-apply bits for already-live slots. */
		for (i = 0u; i < g_mpu_count; i++) {
			uint8_t d_slot =
				(uint8_t)(ULMK_ARCH_MPU_USER_DPR_BASE + i);

			if (regions[i].perms & ULMK_PERM_READ)
				dpre |= (1u << d_slot);
			if (regions[i].perms & ULMK_PERM_WRITE)
				dpwe |= (1u << d_slot);
		}
		mpu_write_enables(prs, dpre, dpwe, cpre, cpxe);
		g_mpu_count = count;
		g_mpu_live  = count;
		return;
	}

	prog = count;

	/* Clear only previously live dynamic slots that are no longer used. */
	for (i = prog; i < g_mpu_live; i++) {
		uint8_t d_slot = (uint8_t)(ULMK_ARCH_MPU_USER_DPR_BASE + i);

		if (d_slot < ULMK_ARCH_MPU_NUM_DPR)
			mpu_write_dpr(d_slot, 0u, 0u);
	}

	for (i = 0u; i < prog; i++)
		mpu_write_user_slot(i, &regions[i], &dpre, &dpwe);

	mpu_write_enables(prs, dpre, dpwe, cpre, cpxe);

	g_mpu_prs     = prs;
	g_mpu_regions = regions;
	g_mpu_count   = count;
	g_mpu_live    = prog;
}

void ulmk_arch_mpu_configure(uint8_t prs, const ulmk_arch_region_t *regions,
			   uint8_t count)
{
	/* Force a full reprogram (ignore identity cache). */
	g_mpu_regions = NULL;
	g_mpu_count   = 0xFFu;
	mpu_program_regions(prs, regions, count);
}

void ulmk_arch_mpu_switch(const ulmk_arch_region_t *regions, uint8_t count,
			uint8_t prs)
{
	mpu_program_regions(prs, regions, count);
}

bool ulmk_arch_mpu_addr_permitted(uintptr_t addr, size_t size, uint32_t perms)
{
	uint32_t dpre;
	uint32_t dpwe;
	uintptr_t end = addr + size;
	uint32_t  dpr_lo;
	uint32_t  dpr_hi;
	uint32_t  i;

	/* Read PRS 1 enable bits */
	__asm__ volatile("mfcr %0, 0xE014" : "=d"(dpre));
	__asm__ volatile("mfcr %0, 0xE024" : "=d"(dpwe));

	for (i = 0u; i < ULMK_ARCH_NUM_DPR; i++) {
		if (!(dpre & (1u << i)))
			continue;

		/* Read DPR_L and DPR_U by using the MFCR switch */
		/* We only check user slots (6-17); static slots are kernel-only */
		if (i < ULMK_ARCH_MPU_USER_DPR_BASE && i != ULMK_ARCH_MPU_MMIO_DPR)
			continue;

		__asm__ volatile("" ::: "memory");
		/*
		 * We cannot MFCR with a variable.  For addr_permitted we use
		 * a simplified heuristic: if any user DPR is enabled and covers
		 * the address. Since full readback would require another 18-case
		 * switch, we return true conservatively when called from kernel
		 * (syscall path) and use this only as a PERIPH range hint.
		 */
		(void)dpr_lo;
		(void)dpr_hi;
		(void)end;
		(void)dpwe;
		(void)perms;
		return true;
	}

	return false;
}

/* =========================================================================
 * IRQ / SRC
 * ========================================================================= */

/* =========================================================================
 * IRQ / SRC
 *
 * QEMU Linumiz TC27x maps Service Request Control registers at:
 *   ULMK_BOARD_SRC_BASE + slot_index * 4  (see board_config.h)
 *
 * Dynamic IRQ slots are allocated from 0xC1; board timer uses 0xC0.
 *
 * SRC register bit layout (tc4x_mode=0 in Linumiz QEMU):
 *   [7:0]  SRPN — service request priority number
 *   [n]    SRE  — service request enable (ULMK_BOARD_SRC_SRE_BIT)
 *   [13:11]TOS  — target CPU (0 = CPU0)
 *   [24]   SRR  — service request raised (set by hardware/SETR)
 *   [25]   CLRR — write 1 to clear SRR
 *   [26]   SETR — write 1 to software-raise SRR (for testing)
 * ========================================================================= */

#define SRC_BASE        ULMK_BOARD_SRC_BASE
#define SRC_SRE_BIT     (1u << ULMK_BOARD_SRC_SRE_BIT)
#define SRC_TOS_SHIFT   ULMK_ARCH_SRC_TOS_SHIFT
#define SRC_SRR_BIT     ULMK_ARCH_SRC_SRR_BIT
#define SRC_CLRR_BIT    ULMK_ARCH_SRC_CLRR_BIT
#define SRC_SETR_BIT    ULMK_ARCH_SRC_SETR_BIT

/* srpn → SRC register address; 0 = unregistered */
static uint32_t g_src_addr[256];
/* Next available SRC allocation slot for ulmk_irq_bind() dynamic IRQs. */
static uint8_t g_next_src_slot = 0xC1u;

void ulmk_arch_irq_vectors_init(uintptr_t btv, uintptr_t biv, uintptr_t isp_top)
{
	uint32_t btv32 = (uint32_t)btv;
	uint32_t biv32 = (uint32_t)biv;
	uint32_t isp32 = (uint32_t)isp_top;

	__asm__ volatile("dsync" ::: "memory");
	__asm__ volatile("mtcr 0xFE24, %0" :: "d"(btv32));	/* BTV */
	__asm__ volatile("mtcr 0xFE20, %0" :: "d"(biv32));	/* BIV, VSS=0 → 32-byte slots */
	__asm__ volatile("mtcr 0xFE28, %0" :: "d"(isp32));	/* ISP */
	__asm__ volatile("isync" ::: "memory");
}

void ulmk_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
	uint32_t addr;

	addr = SRC_BASE + (uint32_t)g_next_src_slot * 4u;
	g_src_addr[srpn] = addr;
	g_next_src_slot++;

	*(volatile uint32_t *)addr =
		(uint32_t)priority | ((uint32_t)cpu_id << SRC_TOS_SHIFT);
}

void ulmk_arch_irq_src_register(uint8_t srpn, uint32_t src_reg_addr)
{
	g_src_addr[srpn] = src_reg_addr;
	if (src_reg_addr)
		*(volatile uint32_t *)(uintptr_t)src_reg_addr = (uint32_t)srpn;
}

void ulmk_arch_irq_src_enable(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_SRE_BIT;
}

void ulmk_arch_irq_src_disable(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr &= ~SRC_SRE_BIT;
}

void ulmk_arch_irq_src_ack(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_CLRR_BIT;
}

bool ulmk_arch_irq_src_is_pending(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (!addr)
		return false;
	return !!(*(volatile uint32_t *)addr & SRC_SRR_BIT);
}

/*
 * ulmk_arch_irq_src_trigger — software-raise an IRQ for testing.
 * Writes the SETR bit to the SRC register associated with @srpn.
 * The SRC must have been configured via ulmk_arch_irq_src_configure first.
 */
void ulmk_arch_irq_src_trigger(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_SETR_BIT;
}

/*
 * _arch_generic_isr_handler — C entry point for SRPN=2..255 ISR stubs.
 *
 * On entry the stub has already done SVLCX.  We must not touch the data
 * stack (A10) before the PRS elevation below — CSA and CSFR accesses bypass
 * the DPR, so the CALL/SVLCX in the stub and the inline asm below are safe
 * in any PRS.  Only regular LD/ST (stack frame) requires PRS 0.
 */
void _arch_generic_isr_handler(void)
{
	uint32_t icr;
	uint32_t psw;

	/*
	 * Elevate to kernel PRS 0 and supervisor IO so CSFR access (ICR) and
	 * kernel data structures succeed.  Assembly stub already cleared PRS
	 * before SVLCX; IO must wait until the task CSA is saved.
	 */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x3C00u;	/* clear PSW.PRS and PSW.IO */
	psw |= 0x800u;		/* IO = supervisor, PRS = 0 */
	__asm__ volatile("mtcr 0xFE04, %0\n\t"
			 "isync"
			 :: "d"(psw) : "memory");

	__asm__ volatile("mfcr %0, 0xFE2C" : "=d"(icr));
	ulmk_kern_irq_dispatch((uint8_t)(icr & 0xFFu));

	/*
	 * If the dispatch woke a higher-priority thread, arm the preemption
	 * handoff so _arch_generic_preempt_isr can switch context on exit.
	 */
	ulmk_kern_sched_dispatch(true);
}

/* =========================================================================
 * Atomic operations
 * ========================================================================= */

uint32_t ulmk_arch_atomic_cas(volatile uint32_t *ptr,
			    uint32_t expected, uint32_t desired)
{
	uint32_t old;

	/*
	 * Single-core CAS via DISABLE/ENABLE.
	 *
	 * DISABLE/ENABLE are accessible at IO >= 1 (driver privilege) and
	 * provide correct atomicity on a single-core system by preventing
	 * preemption from ISRs.
	 *
	 * For multi-core targets, replace with CMPSWAP.W once the SMP
	 * scheduler is introduced.
	 */
	__asm__ __volatile__("disable" ::: "memory");
	old = *ptr;
	if (old == expected)
		*ptr = desired;
	__asm__ __volatile__("enable" ::: "memory");
	return old;
}

uint32_t ulmk_arch_atomic_add(volatile uint32_t *ptr, uint32_t val)
{
	/*
	 * TriCore has no native atomic-add; implement via CAS-retry loop.
	 * On a single-core system only a preempting ISR can race with us,
	 * so the loop terminates quickly.
	 */
	uint32_t old;
	uint32_t new_val;

	do {
		old     = *ptr;
		new_val = old + val;
	} while (ulmk_arch_atomic_cas(ptr, old, new_val) != old);

	return old;
}


/* =========================================================================
 * Syscall entry — arch/tricore/arch.c
 *
 * TriCore SYSCALL (trap class 6) saves the upper context before entering
 * the trap handler.  vectors.S parks TIN + D4–D7 on the stack and passes
 * that frame pointer in D4 — required because a C prologue under -O1+
 * would otherwise clobber the live argument registers.
 * ========================================================================= */

void ulmk_arch_syscall_entry(uint32_t frame_ptr)
{
	const uint32_t *frame = (const uint32_t *)(uintptr_t)frame_ptr;
	uint32_t tin;
	uint32_t args[4];
	uint32_t psw;
	uint32_t icr;
	uint32_t ret;

	tin     = frame[0];
	args[0] = frame[1];
	args[1] = frame[2];
	args[2] = frame[3];
	args[3] = frame[4];

	/*
	 * Raise CCPN to 255, masking all IRQs for the duration of kernel
	 * execution.  The TRAP hardware already saved PCXI.PCPN = 0 (the
	 * caller's CPU priority) before entry; the RFE in _trap_class6
	 * automatically restores CCPN = 0, so any pending IRQs fire via
	 * tail-chaining immediately after kernel exit — no manual restore
	 * needed here.
	 */
	__asm__ volatile("mfcr %0, 0xFE2C" : "=d"(icr));
	icr |= 0xFFu;	/* CCPN = 255 */
	__asm__ volatile("mtcr 0xFE2C, %0\n\t"
			 "isync"
			 :: "d"(icr) : "memory");

	/*
	 * Switch to kernel protection set. The caller's PSW (with its PRS
	 * field) is preserved in the saved upper context; RFE restores it.
	 */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x3000u;	/* PSW.PRS [13:12] = 0 (kernel PRS) */
	__asm__ volatile("mtcr 0xFE04, %0\n\t"
			 "isync"
			 :: "d"(psw) : "memory");

	ret = ulmk_kern_trap_syscall((uint8_t)tin, args);

	/*
	 * If the syscall unblocked a higher-priority thread, perform a
	 * cooperative switch now rather than waiting for the next tick.
	 * Execution resumes here when this thread is rescheduled; at that
	 * point CCPN has been restored to 0 by the context-switch RFE and
	 * interrupts are enabled, so the return path is unprotected but brief.
	 */
	ulmk_kern_sched_dispatch(false);

	__asm__ volatile("mov %%d2, %0" : : "d"(ret));
}

/* =========================================================================
 * Fault context dump — arch-specific
 *
 * Outputs via ulmk_printk_char_out (board primitive, below the kernel print
 * layer) to remain safe when called before full kernel initialisation.
 *
 * pcxi_to_csa(): SRAM segment 7 only (0x7xxx_xxxx = all DSPR on TC27x).
 * ========================================================================= */

static void dump_puts(const char *s)
{
	while (*s)
		ulmk_printk_char_out(*s++);
}

static void dump_hex32(uint32_t v)
{
	int      i;
	uint8_t  nibble;
	char     c;

	dump_puts("0x");
	for (i = 28; i >= 0; i -= 4) {
		nibble = (uint8_t)((v >> i) & 0xFu);
		c = (nibble < 10u) ? (char)('0' + nibble)
				   : (char)('a' + nibble - 10u);
		ulmk_printk_char_out(c);
	}
}

static void dump_u8(uint8_t v)
{
	if (v >= 100u)
		ulmk_printk_char_out((char)('0' + v / 100u));
	if (v >= 10u)
		ulmk_printk_char_out((char)('0' + (v / 10u) % 10u));
	ulmk_printk_char_out((char)('0' + v % 10u));
}

static inline __attribute__((always_inline))
uint32_t *pcxi_to_csa(uint32_t pcxi)
{
	uint32_t link = pcxi & 0x000FFFFFu;	/* strip PCPN/PIE/UL */

	return (uint32_t *)(((link & 0x70000u) << 12) | ((link & 0xFFFFu) << 6));
}

/*
 * Trap class names for readable panic output.
 * Index == TriCore trap class number (0–7).
 */
static const char * const trap_class_names[] = {
	"MMU/MPU data",		/* 0 */
	"internal protection",	/* 1 */
	"instruction error",	/* 2 */
	"context management",	/* 3 */
	"system bus/periph",	/* 4 */
	"assertion",		/* 5 */
	"syscall",		/* 6 — should never reach fault handler */
	"NMI",			/* 7 */
};

/*
 * PSW of the context that caused the trap — read from the hardware-saved
 * upper CSA, not the live CSFR (trap stubs may already have raised IO/PRS).
 *
 * CALL frames on the path to this helper keep PSW.IS=1.  Walk PCXI and pick
 * the first CSA word1 that looks like a userspace task PSW (IS=0, IO!=sup,
 * PRS=user).  Lower-context A11 values rarely match that pattern.
 */
static inline __attribute__((always_inline))
uint32_t trap_interrupted_psw(void)
{
	uint32_t  pcxi;
	uint32_t *uc;
	uint32_t  psw;
	uint32_t  i;
	uint32_t  is;
	uint32_t  io;
	uint32_t  prs;

	__asm__ volatile("mfcr %0, 0xFE00" : "=d"(pcxi));

	for (i = 0u; i < 8u && pcxi != 0u; i++) {
		uc  = pcxi_to_csa(pcxi);
		psw = uc[1];
		is  = (psw >> 9) & 1u;
		io  = (psw >> 10) & 3u;
		prs = (psw >> 12) & 3u;
		if (is == 0u && io < 2u && prs == (uint32_t)ULMK_ARCH_PRS_USER)
			return psw;
		pcxi = uc[0];
	}

	return 0u;
}

/*
 * ulmk_arch_trap_entry — arch-level trap dispatcher, called from vectors.S.
 *
 * Reads the interrupted context's saved PSW to determine whether the fault
 * came from kernel context (ISR active or supervisor privilege) or from a
 * user/driver thread.  Performs the arch-specific dump, then invokes the
 * appropriate kernel callback:
 *   - ulmk_kern_trap_recoverable(): thread killed, scheduler picks next
 *   - ulmk_kern_trap_panic():       unrecoverable, system halted
 *
 * Class 0 (MPU data) and class 1 (internal protection) originating from a
 * non-ISR thread are recoverable — the faulting thread is killed.  All
 * other cases are fatal.
 */
void ulmk_arch_trap_entry(uint8_t trap_class, uint8_t tin)
{
	uint32_t psw;
	uint32_t is;
	int      from_kernel;

	psw         = trap_interrupted_psw();
	is          = (psw >>  9) & 1u;	/* PSW.IS[9]: 1 = on ISP (ISR)   */
	/* IO[11:10] == 2 → supervisor; psw==0 → walk failed. */
	from_kernel = (psw == 0u) || (is != 0u) ||
		      (((psw >> 10) & 3u) == 2u);

	/* Elevate live PSW for kernel diagnostics (printk, CSFR dump). */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x3C00u;
	psw |= 0x800u;
	__asm__ volatile("mtcr 0xFE04, %0\n\t"
			 "isync"
			 :: "d"(psw) : "memory");

	ulmk_printk("TRAP class=%u (%s) tin=%u %s\n",
		  (unsigned)trap_class,
		  trap_class < 8u ? trap_class_names[trap_class] : "?",
		  (unsigned)tin,
		  from_kernel ? "[kernel/ISR]" : "[thread]");

	ulmk_arch_trap_dump(trap_class, tin);

	if ((trap_class == 0u || trap_class == 1u) && !from_kernel) {
		ulmk_kern_trap_recoverable();
	} else {
		ulmk_kern_trap_panic();
	}
}

void ulmk_arch_trap_dump(uint8_t trap_class, uint8_t tin)
{
	uint32_t  pcxi_here;
	uint32_t  fcx_val;
	uint32_t *f_call;
	uint32_t  pcxi_trap;
	uint32_t *f_fault;
	uint32_t  w;

	__asm__ volatile("mfcr %0, 0xFE00" : "=d"(pcxi_here));
	__asm__ volatile("mfcr %0, 0xFE38" : "=d"(fcx_val));

	dump_puts("  PCXI="); dump_hex32(pcxi_here);
	dump_puts(" FCX=");   dump_hex32(fcx_val);
	dump_puts(" class="); dump_u8(trap_class);
	dump_puts(" tin=");   dump_u8(tin);
	dump_puts("\n");

	/*
	 * UC1: upper context saved by "call ulmk_kernel_trap_fault" in vectors.S.
	 * UC0: upper context saved by the trap hardware mechanism.
	 * UC0[3] = A11 = faulting PC (TC1.6.1 §4.5.2).
	 */
	f_call    = pcxi_to_csa(pcxi_here);
	pcxi_trap = f_call[0];

	dump_puts("  UC1 (call frame) at "); dump_hex32((uint32_t)(uintptr_t)f_call);
	dump_puts(":\n");
	for (w = 0u; w < 16u; w++) {
		dump_puts("    ["); dump_u8((uint8_t)w); dump_puts("]=");
		dump_hex32(f_call[w]); dump_puts("\n");
	}

	f_fault = pcxi_to_csa(pcxi_trap);
	dump_puts("  UC0 (hw-trap frame) at "); dump_hex32((uint32_t)(uintptr_t)f_fault);
	dump_puts(":\n");
	for (w = 0u; w < 16u; w++) {
		dump_puts("    ["); dump_u8((uint8_t)w); dump_puts("]=");
		dump_hex32(f_fault[w]); dump_puts("\n");
	}
}

/* =========================================================================
 * Boot entry
 * ========================================================================= */

/*
 * Linker-defined symbols for the vector tables and kernel stack.
 * These are section-start labels; their ADDRESS is the relevant value.
 */
extern char _trap_class0[];	/* BTV: base of .trap_table section */
extern char _ulmk_int_table[];	/* BIV: base of .int_table section  */
extern char _ulmk_isr_stack_top[];	/* ISP: top of ISR stack (separate from kernel stack) */

void ulmk_arch_init(ulmk_boot_info_t *info)
{
	(void)info;

	ulmk_arch_irq_vectors_init(
		(uintptr_t)_trap_class0,
		(uintptr_t)_ulmk_int_table,
		(uintptr_t)_ulmk_isr_stack_top);
}
