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
#include <ul_arch.h>
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
	__asm__ volatile("wait");
}

void ul_arch_cpu_halt(void)
{
	for (;;)
		;
}

uint32_t ul_arch_cpu_clz(uint32_t val)
{
	(void)val;
	return 32; /* TODO: CLZ instruction inline */
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
 * IMPORTANT: The QEMU Linumiz fork inverts upper/lower register sets
 * relative to the TC1.6.1 specification:
 *
 *   Upper context (CALL/RFE): PCXI PSW A10 A11 D8-D11 A12-A15 D12-D15
 *   Lower context (SVLCX/RSLCX): PCXI A11 A2 A3 D0-D3 A4-A7 D4-D7
 *
 * RFE sequence in QEMU: PC = A11 (pre-restore) → load upper context.
 * Therefore lower_csa[1] = trampoline sets the jump target of RFE.
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
	 * GW and CDE are inherited from the kernel PSW.  QEMU Linumiz starts
	 * with IS=1 in the reset PSW and never clears it, so we must strip IS
	 * explicitly; leaving IS=1 causes a class-4 PSE trap after the first
	 * RFE into a fabricated context.
	 */
	__asm__ volatile("mfcr %0, 0xFE04" : "=d"(psw));
	psw &= ~0x7Fu;					/* clear CDC */
	psw &= ~0x200u;					/* clear IS (task stack) */
	psw &= ~0xC00u;					/* clear IO bits */
	psw |= (uint32_t)(priv == UL_PRIV_USER   ? 0u :
			  priv == UL_PRIV_DRIVER ? 0x400u : 0x800u);

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
	lower_csa[0] = upper_link | UL_CSA_UL_FLAG;
	lower_csa[1] = (uint32_t)(uintptr_t)_ul_thread_trampoline;
	lower_csa[8] = (uint32_t)(uintptr_t)arg;	/* A4: pointer arg */

	ctx->pcxi = lower_link;
}

/*
 * Free the saved CSA chain back to FCX.
 * Called when a thread is destroyed.  The chain must not be active
 * (i.e. the thread must be in DEAD state before this is called).
 *
 * TODO: implement — walk the PCXI chain from ctx->pcxi, return each frame
 * to FCX.  Needed for ul_thread_kill / ul_kern_exit.
 */
void ul_arch_ctx_free(ul_arch_ctx_t *ctx)
{
	(void)ctx;
}

/* =========================================================================
 * MPU
 * ========================================================================= */

void ul_arch_mpu_init(void)
{
	/* TODO: set DCON0 coarse mode, clear all DPR/CPR ranges */
}

void ul_arch_mpu_enable(void)
{
	/* TODO: set DCON0.DPM = protection-enabled */
}

void ul_arch_mpu_disable(void)
{
	/* TODO: set DCON0.DPM = off */
}

void ul_arch_mpu_configure(uint8_t prs, const ul_arch_region_t *regions,
			   uint8_t count)
{
	(void)prs;
	(void)regions;
	(void)count;
	/* TODO: program DPRL/DPRH or CPRL/CPRH for the given PRS */
}

void ul_arch_mpu_switch(const ul_arch_region_t *regions, uint8_t count,
			uint8_t prs)
{
	(void)regions;
	(void)count;
	(void)prs;
	/* TODO: MTCR PSW to switch active PRS on context switch */
}

bool ul_arch_mpu_addr_permitted(uintptr_t addr, size_t size, uint32_t perms)
{
	(void)addr;
	(void)size;
	(void)perms;
	return false; /* TODO */
}

/* =========================================================================
 * IRQ / SRC
 * ========================================================================= */

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
	(void)srpn;
	(void)priority;
	(void)cpu_id;
	/* Generic SRC configure not implemented; use peripheral-specific init. */
}

void ul_arch_irq_src_enable(uint8_t srpn)
{
	(void)srpn;
}

void ul_arch_irq_src_disable(uint8_t srpn)
{
	(void)srpn;
}

void ul_arch_irq_src_ack(uint8_t srpn)
{
	(void)srpn;
}

bool ul_arch_irq_src_is_pending(uint8_t srpn)
{
	(void)srpn;
	return false;
}

/* =========================================================================
 * Tick timer — STM0 tickless implementation
 *
 * Register accessors map directly to the STM0 MMIO addresses.
 * The QEMU Linumiz fork places STM0 at 0xF0000000 (real TC27x: 0xF0001000).
 * ========================================================================= */

static volatile uint32_t g_tick_count __attribute__((section(".bss.g_tick_count")));

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
	/*
	 * CMCON: MSTART0=0, MSIZE0=31 → compare all 32 bits of TIM0 against CMP0.
	 * Write before enabling SRC so no spurious match fires.
	 */
	stm0_write(UL_ARCH_STM0_CMCON, 0x0000001Fu);

	/*
	 * Configure SRC_STM0SR0.
	 * The Linumiz QEMU IR uses slot index 0xC0 for STM0 SR0 (not the
	 * hardware-correct 0xF0038490).  TC27x machines have tc4x_mode=0 in
	 * the IR struct, so irq_evaluate checks SRE at bit 10 (not bit 23).
	 * See arch_config.h for the full Linumiz SRC field layout.
	 */
	*(volatile uint32_t *)UL_ARCH_SRC_STM0_SR0 = UL_ARCH_SRC_CONFIG_VAL;

	/* ICR: leave CMP0EN=0; caller arms with ul_arch_tick_deadline(). */
	stm0_write(UL_ARCH_STM0_ICR, 0u);
}

uint32_t ul_arch_tick_get(void)
{
	return stm0_read(UL_ARCH_STM0_TIM0) / UL_ARCH_STM_TICKS_PER_US;
}

void ul_arch_tick_deadline(uint32_t delta_us)
{
	uint32_t now = stm0_read(UL_ARCH_STM0_TIM0);
	uint32_t target = now + delta_us * UL_ARCH_STM_TICKS_PER_US;

	/* Write CMP0 first — QEMU timer_update arms the QEMU timer on this write. */
	stm0_write(UL_ARCH_STM0_CMP0, target);

	/* Enable CMP0EN: interrupt fires when TIM0 reaches CMP0. */
	stm0_write(UL_ARCH_STM0_ICR, 0x00000001u);
}

uint32_t ul_arch_tick_count(void)
{
	return g_tick_count;
}

/*
 * Called from vectors.S tick ISR (after svlcx, before rslcx/rfe).
 * Interrupt priority is already held by hardware (CCPN = UL_ARCH_TICK_SRPN).
 */
void _arch_tick_isr_handler(void)
{
	/* Ack CMP0 match to prevent re-trigger when CMP0EN is re-enabled. */
	stm0_write(UL_ARCH_STM0_ISCR, 0x00000001u);

	/* Disable CMP0EN: one-shot; caller re-arms via ul_arch_tick_deadline(). */
	stm0_write(UL_ARCH_STM0_ICR, 0u);

	g_tick_count++;

	ul_kernel_tick();
}

/* =========================================================================
 * Atomic operations
 * ========================================================================= */

uint32_t ul_arch_atomic_cas(volatile uint32_t *ptr,
			    uint32_t expected, uint32_t desired)
{
	(void)ptr;
	(void)expected;
	(void)desired;
	return 0; /* TODO: CMPSWAP.W instruction */
}

uint32_t ul_arch_atomic_add(volatile uint32_t *ptr, uint32_t val)
{
	(void)ptr;
	(void)val;
	return 0; /* TODO: LDMST or SWAP.W + loop */
}

/* =========================================================================
 * Physical allocator
 * ========================================================================= */

void ul_arch_phys_alloc_init(uintptr_t pool_base, size_t pool_size)
{
	(void)pool_base;
	(void)pool_size;
	/* TODO: first-fit metadata init */
}

void *ul_arch_phys_alloc(size_t size, size_t align)
{
	(void)size;
	(void)align;
	return NULL; /* TODO */
}

void ul_arch_phys_free(void *ptr, size_t size)
{
	(void)ptr;
	(void)size;
	/* TODO */
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

	__asm__ volatile("mov %0, %%d15" : "=d"(tin));
	__asm__ volatile("mov %0, %%d4"  : "=d"(args[0]));
	__asm__ volatile("mov %0, %%d5"  : "=d"(args[1]));
	__asm__ volatile("mov %0, %%d6"  : "=d"(args[2]));
	__asm__ volatile("mov %0, %%d7"  : "=d"(args[3]));

	uint32_t ret = ul_kernel_trap_syscall((uint8_t)tin, args);

	__asm__ volatile("mov %%d2, %0" : : "d"(ret));
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
extern char _ul_kernel_stack_top[];	/* ISP: top of kernel stack    */

void ul_arch_init(ul_boot_info_t *info)
{
	(void)info;

	ul_arch_irq_vectors_init(
		(uintptr_t)_trap_class0,
		(uintptr_t)_ul_int_table,
		(uintptr_t)_ul_kernel_stack_top);
}
