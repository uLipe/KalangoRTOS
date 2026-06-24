/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Platform-independent kernel entry — kernel/kernel_main.c
 * Called by startup.S after ul_arch_init() fills the boot_info.
 * Does not return.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <ul_arch.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_irq_internal.h>
#include <kernel/include/ul_printk.h>
#include <kernel/syscall/syscall_router.h>

/* =========================================================================
 * Arch callbacks — invoked from arch/tricore/vectors.S
 * ========================================================================= */

/*
 * ul_kernel_tick — called from the STM compare-match ISR.
 * Runs in supervisor context with the ISR stack (ISP register).
 */
void ul_kernel_tick(void)
{
	/* TODO: ul_sched_tick(), software timer expiry */
}

/*
 * ul_kernel_trap_syscall — arch-agnostic syscall dispatcher.
 *
 * Called by ul_arch_syscall_entry() (arch/tricore/arch.c) after it has
 * read D15 (TIN) and D4–D7 (args) from the live TriCore registers.
 * This function contains zero arch-specific code.
 *
 * Returns the value that ul_arch_syscall_entry() will write into D2.
 */
uint32_t ul_kernel_trap_syscall(uint8_t tin, uint32_t args[4])
{
	return ul_syscall_router(tin, args[0], args[1], args[2], args[3]);
}

/*
 * ul_kernel_trap_fault — hardware protection fault handler.
 * @trap_class: TriCore trap class (1 = Internal Protection, 3 = FCU, etc.)
 * @tin:        fault subtype (arch_api_spec.md §13.5)
 *
 * Called from vectors.S trap handlers.  Does not return.
 */
void ul_kernel_trap_fault(uint8_t trap_class, uint8_t tin)
{
	(void)trap_class;
	(void)tin;
	/*
	 * TODO:
	 *   - Identify faulting thread from PCXI saved context
	 *   - Kill the faulting thread / notify root thread via IPC
	 */
	for (;;)
		;
}

/* =========================================================================
 * Kernel main — does not return
 * ========================================================================= */

void ul_kernel_main(const ul_boot_info_t *info)
{
	ul_printk("ulipeMicroKernel: kernel entry\n");

	ul_sched_init();
	UL_LOG_DBG("sched init done");

	ul_irq_table_init();
	UL_LOG_DBG("irq table init done");

	ul_printk("ulipeMicroKernel: launching root thread\n");

	/*
	 * TODO:
	 *   1. ul_arch_phys_alloc_init(_ul_user_pool_start, pool_size)
	 *   2. ul_arch_mpu_enable()
	 *   3. Allocate root thread stack
	 *   4. ul_kern_thread_spawn() with ul_root_thread as entry
	 *   5. ul_arch_tick_init(info->tick_hz)
	 *   6. ul_arch_cpu_irq_enable()
	 *   7. ul_arch_ctx_switch(NULL, root_thread_ctx)  ← never returns
	 */

	ul_root_thread(info);

	for (;;)
		ul_arch_cpu_idle();
}
