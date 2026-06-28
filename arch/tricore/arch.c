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
	__asm__ volatile("nop");
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
 * Walks the PCXI chain starting at ctx->pcxi.  Each frame's word 0 is the
 * link to the next frame in the chain (with UL/PIE metadata in bits[20:19]).
 * csa_link_to_addr() strips those bits, so the walk is uniform for upper and
 * lower context frames.
 *
 * Each frame is prepended to the FCX free list.  Interrupts must be disabled
 * by the caller or the entire operation must be atomic w.r.t. the ISR (both
 * csa_alloc in the ISR and this function touch FCX).  On single-core QEMU
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
 * Only useful in test environments (QEMU); the SRC must have been
 * configured via ul_arch_irq_src_configure first.
 */
void ul_arch_irq_src_trigger(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	if (addr)
		*(volatile uint32_t *)addr |= SRC_SETR_BIT;
}

/*
 * _arch_generic_isr_handler — C entry point for SRPN=2..255 ISR stubs.
 * Called from vectors.S with ICR in D4; SRPN = ICR[7:0] = ICR.CCPN.
 */
void _arch_generic_isr_handler(uint32_t icr_val)
{
	ul_kernel_irq_dispatch((uint8_t)(icr_val & 0xFFu));
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
extern char _ul_kernel_stack_top[];	/* ISP: top of kernel stack    */

void ul_arch_init(ul_boot_info_t *info)
{
	(void)info;

	ul_arch_irq_vectors_init(
		(uintptr_t)_trap_class0,
		(uintptr_t)_ul_int_table,
		(uintptr_t)_ul_kernel_stack_top);
}
