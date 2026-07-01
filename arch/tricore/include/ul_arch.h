/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Architecture abstraction layer — TriCore TC1.6.1 / TC2xx
 * Full specification: docs/arch_api_spec.md
 *
 * This header declares the contract between the platform-independent kernel
 * and the TriCore arch port.  All functions here are implemented in
 * arch/tricore/arch.c (or arch_*.c sub-files).
 */

#ifndef UL_ARCH_H
#define UL_ARCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ul/microkernel.h>
#include <arch_config.h>

/* =========================================================================
 * Arch types (arch_api_spec.md §3)
 * ========================================================================= */

/*
 * Saved CPU context — head of the CSA chain for this thread.
 *
 * On suspension ul_arch_ctx_switch() stores the PCXI register here.
 * PCXI encodes a two-frame chain:
 *   pcxi → lower-context CSA (UL=0, saved by SVLCX)
 *        → upper-context CSA (UL=1, saved by CALL into ctx_switch)
 *
 * For a freshly created thread ul_arch_ctx_init() fabricates the same
 * two-frame chain so that the first RSLCX+RFE starts the thread at its
 * entry point via the arch trampoline.
 *
 * Must be the first field so that ul_thread_t.pcxi is accessible at a
 * fixed offset (required by the assembly context-switch path).
 */
typedef struct {
	uint32_t pcxi;
} ul_arch_ctx_t;

/* Saved interrupt state (ICR register value on TriCore). */
typedef uint32_t ul_arch_irq_key_t;

/* Single MPU region descriptor. */
typedef struct {
	uintptr_t base;
	size_t    size;
	uint32_t  perms;	/* UL_PERM_* */
	uint8_t   type;		/* UL_REGION_* */
} ul_arch_region_t;

/* Region type tags */
#define UL_REGION_CODE		0
#define UL_REGION_DATA		1
#define UL_REGION_STACK		2
#define UL_REGION_HEAP		3
#define UL_REGION_PERIPH	4
#define UL_REGION_SHARED	5

/* =========================================================================
 * CPU control (arch_api_spec.md §5)
 * ========================================================================= */

ul_arch_irq_key_t ul_arch_cpu_irq_save(void);
void              ul_arch_cpu_irq_restore(ul_arch_irq_key_t key);
void              ul_arch_cpu_irq_enable(void);
void              ul_arch_cpu_irq_disable(void);
void              ul_arch_cpu_idle(void);
void              ul_arch_cpu_halt(void);
uint32_t          ul_arch_cpu_clz(uint32_t val);

/* =========================================================================
 * Context management (arch_api_spec.md §6)
 * ========================================================================= */

void ul_arch_csa_pool_init(uintptr_t pool_base, size_t pool_size);

void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *arg), void *arg,
		      uintptr_t stack_top, ul_privilege_t priv);

void ul_arch_ctx_switch(ul_arch_ctx_t *from, const ul_arch_ctx_t *to);

void ul_arch_ctx_free(ul_arch_ctx_t *ctx);

/* =========================================================================
 * MPU (arch_api_spec.md §7)
 * ========================================================================= */

void ul_arch_mpu_init(void);
void ul_arch_mpu_enable(void);
void ul_arch_mpu_disable(void);

void ul_arch_mpu_configure(uint8_t prs, const ul_arch_region_t *regions,
			   uint8_t count);

void ul_arch_mpu_switch(const ul_arch_region_t *regions, uint8_t count,
			uint8_t prs);

bool ul_arch_mpu_addr_permitted(uintptr_t addr, size_t size,
				uint32_t perms);

/* =========================================================================
 * IRQ and SRC control (arch_api_spec.md §8)
 * ========================================================================= */

void ul_arch_irq_vectors_init(uintptr_t btv, uintptr_t biv,
			      uintptr_t isp_top);

void ul_arch_irq_src_configure(uint8_t srpn, uint8_t priority,
			       uint8_t cpu_id);

void ul_arch_irq_src_enable(uint8_t srpn);
void ul_arch_irq_src_disable(uint8_t srpn);
void ul_arch_irq_src_ack(uint8_t srpn);
bool ul_arch_irq_src_is_pending(uint8_t srpn);

/*
 * ul_arch_irq_src_trigger — software-raise an IRQ (QEMU / test use only).
 * Sets the SETR bit in the SRC register for @srpn.
 */
void ul_arch_irq_src_trigger(uint8_t srpn);

/* =========================================================================
 * Tick timer — periodic, STM0 CMP0 compare-match (arch_api_spec.md §9)
 *
 * ul_arch_tick_init — configure STM0 and arm the first periodic interrupt.
 * ul_arch_tick_get  — returns elapsed µs since reset (wraps at ~4294s).
 * ========================================================================= */

void     ul_arch_tick_init(void);
uint32_t ul_arch_tick_get(void);

/* =========================================================================
 * Atomic operations (arch_api_spec.md §10)
 * ========================================================================= */

uint32_t ul_arch_atomic_cas(volatile uint32_t *ptr,
			    uint32_t expected, uint32_t desired);

uint32_t ul_arch_atomic_add(volatile uint32_t *ptr, uint32_t val);

/* =========================================================================
 * Physical memory allocator (arch_api_spec.md §11 — support)
 * ========================================================================= */


/* =========================================================================
 * Boot entry (arch_api_spec.md §11)
 * ========================================================================= */

/*
 * ul_board_init - optional board-level hardware setup (weak no-op stub)
 * Called by startup.S before .data copy.  User overrides with strong symbol.
 */
__attribute__((weak)) void ul_board_init(void);

/*
 * ul_arch_init - one-time CPU and peripheral initialisation
 * Called by startup.S after ul_board_init() and after .data/.bss init.
 * Fills @info and returns; control passes to ul_kernel_main().
 */
void ul_arch_init(ul_boot_info_t *info);

/* =========================================================================
 * Arch-internal syscall entry (arch/tricore/arch.c)
 * Reads D15 (TIN) and D4–D7 (args) from live registers and dispatches
 * to ul_kernel_trap_syscall().  Called from _trap_class6 in vectors.S.
 * ========================================================================= */

void ul_arch_syscall_entry(void);

/*
 * ul_arch_trap_dump — dump arch-specific CPU state after a hardware fault.
 * @trap_class / @tin: forwarded from the trap vector for context.
 *
 * Outputs directly via ul_printk_char_out (a board primitive below the kernel
 * print layer) so it stays safe even when called before the kernel is fully
 * initialised.  Called by ul_kernel_trap_fault() in kernel_main.c.
 */
void ul_arch_trap_dump(uint8_t trap_class, uint8_t tin);

/*
 * ul_printk_char_out — single-character output primitive.
 * Provided by the board (e.g. boards/qemu_tc27x/qemu_console.c) as a weak
 * symbol.  Both the kernel print subsystem and arch fault handlers may call
 * this directly; it is declared here so the arch layer can use it without
 * depending on kernel-internal headers.
 */
void ul_printk_char_out(char c);

/* =========================================================================
 * Kernel callbacks — implemented by kernel/, called by arch port
 * (arch_api_spec.md §12)
 *
 * These functions contain NO arch-specific code.  The arch port is
 * responsible for extracting all hardware-dependent state before calling.
 * ========================================================================= */

/*
 * ul_kernel_tick — advance the scheduler clock by one tick.
 * Called from the tick timer ISR before RSLCX/RFE.
 */
void ul_kernel_tick(void);

/*
 * ul_kernel_irq_dispatch — route a hardware IRQ to its notification object.
 * @srpn: SRPN (priority number) of the interrupt that fired.
 * Called from the generic ISR stub before RSLCX/RFE.
 */
void ul_kernel_irq_dispatch(uint8_t srpn);

/*
 * ul_kernel_irq_check_preempt — check if the just-dispatched IRQ woke a
 * higher-priority thread and arm the preemption handoff if so.
 * Called from _arch_generic_isr_handler after ul_kernel_irq_dispatch.
 */
void ul_kernel_irq_check_preempt(void);

/*
 * ul_kernel_syscall_check_preempt — check if the syscall woke a higher-
 * priority thread and yield the CPU to it before returning to userspace.
 * Called from ul_arch_syscall_entry() after the syscall handler returns.
 */
void ul_kernel_syscall_check_preempt(void);

/*
 * ul_kernel_trap_syscall — dispatch a SYSCALL trap (class 6).
 * @tin:  trap identification number (= syscall number, 0–127)
 * @args: arguments read from D4–D7 by ul_arch_syscall_entry()
 * Returns the value to be written to D2 (done by ul_arch_syscall_entry).
 * Called from ul_arch_syscall_entry() in arch/tricore/arch.c.
 */
uint32_t ul_kernel_trap_syscall(uint8_t tin, uint32_t args[4]);

/*
 * ul_kernel_trap_fault — handle a hardware protection fault.
 * @trap_class: TriCore trap class (1 = Internal Protection, 3 = FCU, etc.)
 * @tin:        fault subtype (MPR=2, MPW=3, MPX=4, MPP=5, MPN=6, GRWP=7)
 * Called from the trap class handler in vectors.S.  Does not return.
 */
void ul_kernel_trap_fault(uint8_t trap_class, uint8_t tin);

/*
 * ul_kernel_main - platform-independent kernel entry; does not return
 * Called by startup.S after ul_arch_init().
 */
void ul_kernel_main(const ul_boot_info_t *info);

#endif /* UL_ARCH_H */
