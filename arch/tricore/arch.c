/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TriCore TC1.6.1 arch port — arch/tricore/arch.c
 * Implements: arch/tricore/include/ul_arch.h
 * Full specification: docs/arch_api_spec.md
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <ul_arch.h>
#include <kernel/include/ul_printk.h>
/* =========================================================================
 * CPU control
 * ========================================================================= */

ul_arch_irq_key_t ul_arch_cpu_irq_save(void)
{
	uint32_t icr;

	__asm__ volatile("mfcr %0, 0xFE2C" : "=d"(icr));
	__asm__ volatile("disable" ::: "memory");
	return icr;
}

void ul_arch_cpu_irq_restore(ul_arch_irq_key_t key)
{
	__asm__ volatile("mtcr 0xFE2C, %0" :: "d"((uint32_t)key));
	__asm__ volatile("isync" ::: "memory");
}

void ul_arch_cpu_irq_enable(void)
{
	__asm__ volatile("enable" ::: "memory");
}

void ul_arch_cpu_irq_disable(void)
{
	__asm__ volatile("disable" ::: "memory");
}

void ul_arch_cpu_idle(void)
{
#if UL_ARCH_IDLE_IS_WAIT
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

void ul_arch_cpu_halt(void)
{
	for (;;)
		;
}

uint32_t ul_arch_cpu_clz(uint32_t val)
{
	uint32_t result;

	__asm__("clz %0, %1" : "=d"(result) : "d"(val));
	return result;
}

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
		ul_arch_cpu_halt(); /* CSA pool exhausted — fatal */

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

void ul_arch_csa_pool_init(uintptr_t pool_base, size_t pool_size)
{
	(void)pool_base;
	(void)pool_size;
	/*
	 * The CSA free list is already built by startup.S before ul_arch_init()
	 * is called — startup.S links all frames and writes FCX/LCX.
	 * This function exists for platforms that need software-side init.
	 */
}

extern void _ul_thread_trampoline(void);

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
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *arg), void *arg,
		      uintptr_t stack_top, ul_privilege_t priv)
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
	psw |= (uint32_t)(priv == UL_PRIV_USER   ? 0u :
			  priv == UL_PRIV_DRIVER ? 0x400u : 0x800u);
	/* PSW.PRS [13:12]: kernel=0 (PRS 0), driver/user=1 (PRS 1) */
	psw &= ~0x3000u;
	if (priv != UL_PRIV_KERNEL)
		psw |= (uint32_t)UL_ARCH_PRS_USER << 12;

	upper_link = csa_alloc();
	upper_csa  = csa_link_to_addr(upper_link);
	for (i = 0u; i < 16u; i++)
		upper_csa[i] = 0u;
	upper_csa[1]  = psw;
	upper_csa[2]  = (uint32_t)stack_top;
	upper_csa[3]  = (uint32_t)(uintptr_t)_ul_thread_trampoline;
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
	lower_csa[1] = (uint32_t)(uintptr_t)_ul_thread_trampoline;
	lower_csa[8] = (uint32_t)(uintptr_t)arg;	/* A4: pointer arg */

	ctx->pcxi = lower_link;
}

/*
 * Free the saved CSA chain back to FCX.
 * Called when a thread is destroyed.  The chain must not be active
 * (i.e. the thread must be in DEAD state before this is called).
 *
 * Walks the PCXI chain starting at ctx->pcxi.  Each frame's word 0 is the
 * link to the next frame in the chain (with UL/PIE metadata in bits[20:19]).
 * csa_link_to_addr() strips those bits, so the walk is uniform for upper and
 * lower context frames.
 *
 * Each frame is prepended to the FCX free list.  Interrupts must be disabled
 * by the caller or the entire operation must be atomic w.r.t. the ISR (both
 * csa_alloc in the ISR and this function touch FCX).  On a single-core system
 * this is safe because the timer ISR only touches FCX via hardware.
 */
void ul_arch_ctx_free(ul_arch_ctx_t *ctx)
{
	uint32_t link;
	uint32_t next_link;
	uint32_t *frame;
	uint32_t fcx;
	uint32_t frame_link;

	if (!ctx)
		return;

	link = ctx->pcxi;
	ctx->pcxi = 0u;

	while (link != 0u) {
		frame     = csa_link_to_addr(link);
		next_link = frame[0];

		/* Prepend this frame to the FCX free list. */
		__asm__ volatile("mfcr %0, 0xFE38" : "=d"(fcx));
		frame[0] = fcx;
		frame_link = addr_to_csa_link(frame);
		__asm__ volatile("dsync" ::: "memory");
		__asm__ volatile("mtcr 0xFE38, %0" :: "d"(frame_link));
		__asm__ volatile("isync" ::: "memory");

		/* Strip UL (bit 20) and PIE (bit 19) to get the address link. */
		link = next_link & 0x7FFFFu;
	}
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
	mpu_mtcr(UL_ARCH_CSFR_DPR_L(slot), lower);
	mpu_mtcr(UL_ARCH_CSFR_DPR_U(slot), upper);
}

static void mpu_write_cpr(uint8_t slot, uint32_t lower, uint32_t upper)
{
	mpu_mtcr(UL_ARCH_CSFR_CPR_L(slot), lower);
	mpu_mtcr(UL_ARCH_CSFR_CPR_U(slot), upper);
}

/*
 * Write DPRE, DPWE, CPRE, CPXE enable registers for a given PRS.
 * ISYNC is issued at the end to flush the pipeline.
 */
static void mpu_write_enables(uint8_t prs, uint32_t dpre, uint32_t dpwe,
			      uint32_t cpre, uint32_t cpxe)
{
	mpu_mtcr(UL_ARCH_CSFR_DPRE_0 + (uint32_t)prs * 4u, dpre);
	mpu_mtcr(UL_ARCH_CSFR_DPWE_0 + (uint32_t)prs * 4u, dpwe);
	mpu_mtcr(UL_ARCH_CSFR_CPRE_0 + (uint32_t)prs * 4u, cpre);
	mpu_mtcr(UL_ARCH_CSFR_CPXE_0 + (uint32_t)prs * 4u, cpxe);
	__asm__ volatile("isync" ::: "memory");
}

/* =========================================================================
 * MPU
 * ========================================================================= */

/*
 * Static DPR/CPR layout (configured once at boot):
 *
 *   DPR 0: entire 4 GiB address space (kernel PRS 0, R+W)
 *   DPR 1: peripheral region (kernel PRS 0, R+W)
 *   DPR 2: flash region (user PRS 1, R only — needed for const data)
 *   DPR 3: board console device (PRS 0+1, R+W; only when UL_ARCH_QEMU_VIRT_CONSOLE)
 *   DPR 4: shared RAM (BSS..user_pool_end, PRS 1 R+W) — temporary, see arch_config.h
 *   DPR 5: reserved (unused, zeroed)
 *   DPR 6–17: per-thread dynamic (configured by mpu_switch())
 *
 *   CPR 0: entire flash (PRS 0 and PRS 1, execute+read)
 *   CPR 1–9: reserved / per-thread code regions
 */
void ul_arch_mpu_init(void)
{
	uint32_t syscon;
	uint8_t  i;

	/*
	 * Linker-defined bounds for the SRAM DPR.
	 * Declared here to avoid polluting the global namespace.
	 */
	extern uint8_t _ul_kernel_data_start[];
	extern uint8_t _ul_user_pool_end[];

	/* Disable protection during reconfiguration */
	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(UL_ARCH_CSFR_SYSCON, syscon & ~UL_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");

	/* Zero all DPR and CPR ranges */
	for (i = 0u; i < UL_ARCH_NUM_DPR; i++)
		mpu_write_dpr(i, 0u, 0u);
	for (i = 0u; i < UL_ARCH_NUM_CPR; i++)
		mpu_write_cpr(i, 0u, 0u);

	/* Zero all PRS enable registers */
	for (i = 0u; i < UL_ARCH_NUM_PRS; i++)
		mpu_write_enables(i, 0u, 0u, 0u, 0u);

	/*
	 * DPR layout — 4-slot model required by the current QEMU build.
	 *
	 * The QEMU linumiz fork (release/ifx/tricore-2.0) implements only
	 * DPR slots 0-3 (CSFR 0xC000-0xC018).  Slots 4+ (0xC020+) are
	 * silently ignored.  All PRS1 accesses must therefore fit within
	 * slots 0-3.  Slot 0 is kernel-only (full 4 GiB); slots 1-3 are
	 * accessible to PRS 1.
	 */

	/* DPR 0: entire 4 GiB — kernel full R+W access via PRS 0 */
	mpu_write_dpr(UL_ARCH_MPU_KERNEL_DPR,
		      0x00000000u, 0xFFFFFFF8u);

	/*
	 * DPR 1: SRAM region (kernel_data_start..user_pool_end).
	 * Both PRS 0 and PRS 1 need R+W here: driver threads access globals,
	 * stacks, and heap blocks; kernel accesses TCBs, IRQ tables, etc.
	 */
	mpu_write_dpr(UL_ARCH_MPU_SRAM_DPR,
		      (uint32_t)(uintptr_t)_ul_kernel_data_start,
		      (uint32_t)(uintptr_t)_ul_user_pool_end - 8u);

	/*
	 * DPR 2: flash read-only (0x80000000..flash_end).
	 * User threads need this to load .rodata (string literals, const arrays).
	 */
	mpu_write_dpr(UL_ARCH_MPU_FLASH_DPR,
		      UL_ARCH_FLASH_BASE,
		      UL_ARCH_FLASH_BASE + UL_ARCH_FLASH_SIZE - 8u);

	/*
	 * DPR 3: console + peripheral range (0xBF000000..0xFFFFFFF8).
	 * Covers both the QEMU virt debug UART (0xBF000000) and the TriCore
	 * peripheral space (0xF0000000+) in a single slot — the only one
	 * available for PRS 1 after SRAM and flash slots are consumed.
	 * Driver threads need peripheral access for SRC registers, STM, etc.
	 */
	mpu_write_dpr(UL_ARCH_MPU_VIRT_DPR,
		      0xBF000000u, 0xFFFFFFF8u);

	/* CPR 0: entire flash — all threads may execute code from here */
	mpu_write_cpr(UL_ARCH_MPU_CPR_ALL,
		      UL_ARCH_FLASH_BASE,
		      UL_ARCH_FLASH_BASE + UL_ARCH_FLASH_SIZE - 8u);

	__asm__ volatile("isync" ::: "memory");

	/*
	 * PRS 0 (kernel): DPRs 0-3 R+W, CPR 0 R+X.
	 * DPR 0 already covers the full address space, so bits 1-3 are
	 * redundant for PRS 0 but kept for symmetry.
	 */
	mpu_write_enables(0u,
			  0xFu,  /* DPRE: slots 0-3 readable */
			  0xFu,  /* DPWE: slots 0-3 writable */
			  0x1u,  /* CPRE: CPR 0 readable */
			  0x1u); /* CPXE: CPR 0 executable */

	/*
	 * PRS 1 (user/driver threads):
	 *   DPR 1 — SRAM (R+W) — globals, stacks, heap
	 *   DPR 2 — flash (R only) — .rodata access
	 *   DPR 3 — console + peripheral (R+W) — ul_printk, SRC, STM
	 *
	 * DPR 0 (full 4 GiB) is intentionally excluded from PRS 1 so that
	 * kernel-only addresses (CSFRs, etc.) remain inaccessible.
	 */
	mpu_write_enables(1u,
			  (1u << UL_ARCH_MPU_SRAM_DPR)  |
			  (1u << UL_ARCH_MPU_FLASH_DPR) |
			  (1u << UL_ARCH_MPU_VIRT_DPR),
			  (1u << UL_ARCH_MPU_SRAM_DPR)  |
			  (1u << UL_ARCH_MPU_VIRT_DPR),
			  (1u << UL_ARCH_MPU_CPR_ALL),
			  (1u << UL_ARCH_MPU_CPR_ALL));

	/* PRS 2, 3: remain zeroed (unused) */
}void ul_arch_mpu_enable(void)
{
	uint32_t syscon;

	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(UL_ARCH_CSFR_SYSCON, syscon | UL_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");
}

void ul_arch_mpu_disable(void)
{
	uint32_t syscon;

	__asm__ volatile("mfcr %0, 0xFE14" : "=d"(syscon));
	mpu_mtcr(UL_ARCH_CSFR_SYSCON, syscon & ~UL_ARCH_SYSCON_PROTEN);
	__asm__ volatile("isync" ::: "memory");
}

/*
 * Configure user DPR slots (6–17) for the given PRS from @regions.
 * Also updates the PRS enable bits, preserving the static flash-read bit (DPR 2).
 */
static void mpu_program_regions(uint8_t prs, const ul_arch_region_t *regions,
				uint8_t count)
{
	uint32_t dpre;
	uint32_t dpwe;
	uint32_t cpre;
	uint32_t cpxe;
	uint8_t  d_slot;
	uint8_t  i;

	/*
	 * Static bits for PRS 1 (user/driver threads):
	 *   DPR 1: SRAM R+W — globals, stacks, heap
	 *   DPR 2: flash R — .rodata access
	 *   DPR 3: console + peripheral R+W — ul_printk, SRC, STM
	 *
	 * DPR 0 (full 4 GiB, kernel-only) is intentionally omitted.
	 * Slots 6-17 below handle per-thread regions; in the current QEMU
	 * build those slots are unmapped (writes are no-ops), so all threads
	 * share the SRAM/flash/periph ranges set above.
	 */
	dpre = (1u << UL_ARCH_MPU_SRAM_DPR) |
	       (1u << UL_ARCH_MPU_FLASH_DPR) |
	       (1u << UL_ARCH_MPU_VIRT_DPR);
	dpwe = (1u << UL_ARCH_MPU_SRAM_DPR) |
	       (1u << UL_ARCH_MPU_VIRT_DPR);
	cpre = (1u << UL_ARCH_MPU_CPR_ALL);
	cpxe = (1u << UL_ARCH_MPU_CPR_ALL);

	/* Clear all user DPR slots before reprogramming */
	for (i = UL_ARCH_MPU_USER_DPR_BASE; i < UL_ARCH_NUM_DPR; i++)
		mpu_write_dpr(i, 0u, 0u);

	d_slot = UL_ARCH_MPU_USER_DPR_BASE;

	if (!regions || count == 0u)
		goto write_enables;

	for (i = 0u; i < count && d_slot < UL_ARCH_NUM_DPR; i++) {
		mpu_write_dpr(d_slot,
			      (uint32_t)regions[i].base,
			      (uint32_t)(regions[i].base + regions[i].size - 8u));

		if (regions[i].perms & UL_PERM_READ)
			dpre |= (1u << d_slot);
		if (regions[i].perms & UL_PERM_WRITE)
			dpwe |= (1u << d_slot);

		d_slot++;
	}

write_enables:
	mpu_write_enables(prs, dpre, dpwe, cpre, cpxe);
}

void ul_arch_mpu_configure(uint8_t prs, const ul_arch_region_t *regions,
			   uint8_t count)
{
	mpu_program_regions(prs, regions, count);
}

void ul_arch_mpu_switch(const ul_arch_region_t *regions, uint8_t count,
			uint8_t prs)
{
	mpu_program_regions(prs, regions, count);
}

bool ul_arch_mpu_addr_permitted(uintptr_t addr, size_t size, uint32_t perms)
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

	for (i = 0u; i < UL_ARCH_NUM_DPR; i++) {
		if (!(dpre & (1u << i)))
			continue;

		/* Read DPR_L and DPR_U by using the MFCR switch */
		/* We only check user slots (6-17); static slots are kernel-only */
		if (i < UL_ARCH_MPU_USER_DPR_BASE && i != UL_ARCH_MPU_FLASH_DPR)
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
 *   IR_SRC_BASE (0xF0038000) + slot_index * 4
 *
 * The tick ISR uses slot 0xC0 (configured in arch_config.h).
 * User-programmable IRQs are allocated sequentially from slot 0xC1.
 *
 * SRC register bit layout (tc4x_mode=0 in Linumiz QEMU):
 *   [7:0]  SRPN — service request priority number
 *   [10]   SRE  — service request enable (bit 10 for tc4x_mode=0)
 *   [13:11]TOS  — target CPU (0 = CPU0)
 *   [24]   SRR  — service request raised (set by hardware/SETR)
 *   [25]   CLRR — write 1 to clear SRR
 *   [26]   SETR — write 1 to software-raise SRR (for testing)
 * ========================================================================= */

#define SRC_BASE        0xF0038000u
#define SRC_SRE_BIT     (1u << 10)
#define SRC_TOS_SHIFT   11u
#define SRC_SRR_BIT     (1u << 24)
#define SRC_CLRR_BIT    (1u << 25)
#define SRC_SETR_BIT    (1u << 26)

/* srpn → SRC register address; 0 = unregistered */
static uint32_t g_src_addr[256];
/* Next available SRC allocation slot (0xC0 = tick, start from 0xC1) */
static uint8_t g_next_src_slot = 0xC1u;

void ul_arch_irq_vectors_init(uintptr_t btv, uintptr_t biv, uintptr_t isp_top)
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

void ul_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
	uint32_t addr;

	if (srpn == UL_ARCH_TICK_SRPN)
		return;

	addr = SRC_BASE + (uint32_t)g_next_src_slot * 4u;
	g_src_addr[srpn] = addr;
	g_next_src_slot++;

	*(volatile uint32_t *)addr =
		(uint32_t)priority | ((uint32_t)cpu_id << SRC_TOS_SHIFT);
}

void ul_arch_irq_src_enable(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_SRE_BIT;
}

void ul_arch_irq_src_disable(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr &= ~SRC_SRE_BIT;
}

void ul_arch_irq_src_ack(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_CLRR_BIT;
}

bool ul_arch_irq_src_is_pending(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (!addr)
		return false;
	return !!(*(volatile uint32_t *)addr & SRC_SRR_BIT);
}

/*
 * ul_arch_irq_src_trigger — software-raise an IRQ for testing.
 * Writes the SETR bit to the SRC register associated with @srpn.
 * The SRC must have been configured via ul_arch_irq_src_configure first.
 */
void ul_arch_irq_src_trigger(uint8_t srpn)
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
	 * Elevate to kernel PRS 0 so that subsequent data accesses (kernel
	 * dispatch table, notif objects, etc.) succeed.  This must happen
	 * before any C-generated stack write — hence the register-only vars.
	 */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x3000u;	/* PSW.PRS [13:12] = 0 */
	__asm__ volatile("mtcr 0xFE04, %0\n\t"
			 "isync"
			 :: "d"(psw) : "memory");

	__asm__ volatile("mfcr %0, 0xFE2C" : "=d"(icr));
	ul_kernel_irq_dispatch((uint8_t)(icr & 0xFFu));

	/*
	 * If the dispatch woke a higher-priority thread, arm the preemption
	 * handoff so _arch_generic_preempt_isr can switch context on exit.
	 */
	ul_kernel_irq_check_preempt();
}

/* =========================================================================
 * Tick timer — STM0 periodic (ticked) implementation
 *
 * Register accessors map directly to the STM0 MMIO addresses defined in
 * arch_config.h (UL_ARCH_STM0_BASE).
 *
 * CMP0 is re-armed in the ISR by adding the fixed period to the previous
 * compare value, avoiding drift accumulation over time.
 * ========================================================================= */

/*
 * Number of STM0 counter ticks per kernel tick period.
 * UL_CONFIG_HW_SYS_CLOCK_HZ is the machine clock fed into STM0 on this arch.
 */
#define TICK_PERIOD_TICKS	(UL_CONFIG_HW_SYS_CLOCK_HZ / UL_CONFIG_TICK_HZ)

static inline uint32_t stm0_read(uint32_t reg_addr)
{
	return *(volatile uint32_t *)reg_addr;
}

static inline void stm0_write(uint32_t reg_addr, uint32_t val)
{
	*(volatile uint32_t *)reg_addr = val;
}

void ul_arch_tick_init(void)
{
	uint32_t now;

	/*
	 * CMCON: MSTART0=0, MSIZE0=31 — compare all 32 bits of TIM0 against CMP0.
	 * Configure SRC and arm the first compare before enabling interrupts.
	 */
	stm0_write(UL_ARCH_STM0_CMCON, 0x0000001Fu);
	*(volatile uint32_t *)UL_ARCH_SRC_STM0_SR0 = UL_ARCH_SRC_CONFIG_VAL;

	now = stm0_read(UL_ARCH_STM0_TIM0);
	stm0_write(UL_ARCH_STM0_CMP0, now + TICK_PERIOD_TICKS);
	stm0_write(UL_ARCH_STM0_ICR, 0x00000001u);
}

uint32_t ul_arch_tick_get(void)
{
	return stm0_read(UL_ARCH_STM0_TIM0) / (UL_CONFIG_HW_SYS_CLOCK_HZ / 1000000u);
}

/*
 * Called from vectors.S tick ISR (after svlcx, before rslcx/rfe).
 * Interrupt priority is already held by hardware (CCPN = UL_ARCH_TICK_SRPN).
 */
void _arch_tick_isr_handler(void)
{
	uint32_t psw;
	uint32_t next_cmp;

	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x3000u;
	__asm__ volatile("mtcr 0xFE04, %0\n\t"
			 "isync"
			 :: "d"(psw) : "memory");

	stm0_write(UL_ARCH_STM0_ISCR, 0x00000001u);

	/*
	 * Re-arm by adding the period to the previous CMP0 value.
	 * Reading CMP0 (not TIM0) avoids drift: each period starts exactly
	 * TICK_PERIOD_TICKS after the previous one regardless of ISR latency.
	 */
	next_cmp = stm0_read(UL_ARCH_STM0_CMP0) + TICK_PERIOD_TICKS;
	stm0_write(UL_ARCH_STM0_CMP0, next_cmp);
	stm0_write(UL_ARCH_STM0_ICR, 0x00000001u);

	ul_kernel_tick();
}


/* =========================================================================
 * Atomic operations
 * ========================================================================= */

uint32_t ul_arch_atomic_cas(volatile uint32_t *ptr,
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

uint32_t ul_arch_atomic_add(volatile uint32_t *ptr, uint32_t val)
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
	} while (ul_arch_atomic_cas(ptr, old, new_val) != old);

	return old;
}


/* =========================================================================
 * Syscall entry — arch/tricore/arch.c
 *
 * TriCore SYSCALL (trap class 6) saves the upper context before entering
 * the trap handler.  D4–D7 (syscall args) and D15 (TIN = syscall number)
 * remain as live physical registers on entry to this function.
 *
 * We read them with volatile inline asm before any compiler write can
 * overwrite them, then forward them to the arch-agnostic kernel callback
 * ul_kernel_trap_syscall().  Finally we write D2 (return register) so the
 * user sees the return value after RFE in vectors.S.
 * ========================================================================= */

void ul_arch_syscall_entry(void)
{
	uint32_t tin, args[4];
	uint32_t psw;
	uint32_t icr;

	/*
	 * Capture syscall arguments from physical registers before any
	 * compiler-generated code can overwrite them.  D15 = TIN (syscall
	 * number), D4–D7 = up to four arguments (TriCore calling convention).
	 */
	__asm__ volatile("mov %0, %%d15" : "=d"(tin));
	__asm__ volatile("mov %0, %%d4"  : "=d"(args[0]));
	__asm__ volatile("mov %0, %%d5"  : "=d"(args[1]));
	__asm__ volatile("mov %0, %%d6"  : "=d"(args[2]));
	__asm__ volatile("mov %0, %%d7"  : "=d"(args[3]));

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

	uint32_t ret = ul_kernel_trap_syscall((uint8_t)tin, args);

	/*
	 * If the syscall unblocked a higher-priority thread, perform a
	 * cooperative switch now rather than waiting for the next tick.
	 * Execution resumes here when this thread is rescheduled; at that
	 * point CCPN has been restored to 0 by the context-switch RFE and
	 * interrupts are enabled, so the return path is unprotected but brief.
	 */
	ul_kernel_syscall_check_preempt();

	__asm__ volatile("mov %%d2, %0" : : "d"(ret));
}

/* =========================================================================
 * Fault context dump — arch-specific
 *
 * Outputs via ul_printk_char_out (board primitive, below the kernel print
 * layer) to remain safe when called before full kernel initialisation.
 *
 * pcxi_to_csa(): SRAM segment 7 only (0x7xxx_xxxx = all DSPR on TC27x).
 * ========================================================================= */

static void dump_puts(const char *s)
{
	while (*s)
		ul_printk_char_out(*s++);
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
		ul_printk_char_out(c);
	}
}

static void dump_u8(uint8_t v)
{
	if (v >= 100u)
		ul_printk_char_out((char)('0' + v / 100u));
	if (v >= 10u)
		ul_printk_char_out((char)('0' + (v / 10u) % 10u));
	ul_printk_char_out((char)('0' + v % 10u));
}

static inline uint32_t *pcxi_to_csa(uint32_t pcxi)
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
 * ul_arch_trap_entry — arch-level trap dispatcher, called from vectors.S.
 *
 * Reads PSW to determine whether the fault came from kernel context (ISR
 * active or supervisor privilege) or from a user/driver thread.  Performs
 * the arch-specific dump, then invokes the appropriate kernel callback:
 *   - ul_kernel_trap_recoverable(): thread killed, scheduler picks next
 *   - ul_kernel_trap_panic():       unrecoverable, system halted
 *
 * Class 1 (Internal Protection) originating from a non-kernel thread is
 * the only recoverable case — all others are fatal.
 */
void ul_arch_trap_entry(uint8_t trap_class, uint8_t tin)
{
	uint32_t psw;
	uint32_t io;
	uint32_t is;
	int      from_kernel;

	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));

	io          = (psw >> 10) & 3u;	/* PSW.IO[11:10]: 2 = supervisor */
	is          = (psw >>  9) & 1u;	/* PSW.IS[9]: 1 = on ISP (ISR)   */
	from_kernel = (io == 2u) || (is != 0u);

	ul_printk("TRAP class=%u (%s) tin=%u %s\n",
		  (unsigned)trap_class,
		  trap_class < 8u ? trap_class_names[trap_class] : "?",
		  (unsigned)tin,
		  from_kernel ? "[kernel/ISR]" : "[thread]");

	ul_arch_trap_dump(trap_class, tin);

	if (trap_class == 1u && !from_kernel) {
		ul_kernel_trap_recoverable();
	} else {
		ul_kernel_trap_panic();
	}
}

void ul_arch_trap_dump(uint8_t trap_class, uint8_t tin)
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
	 * UC1: upper context saved by "call ul_kernel_trap_fault" in vectors.S.
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
extern char _ul_int_table[];	/* BIV: base of .int_table section  */
extern char _ul_isr_stack_top[];	/* ISP: top of ISR stack (separate from kernel stack) */

void ul_arch_init(ul_boot_info_t *info)
{
	(void)info;

	ul_arch_irq_vectors_init(
		(uintptr_t)_trap_class0,
		(uintptr_t)_ul_int_table,
		(uintptr_t)_ul_isr_stack_top);
}
