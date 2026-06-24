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

void ul_kernel_tick(void)
{
	/* TODO: ul_sched_tick(), software timer expiry */
}

uint32_t ul_kernel_trap_syscall(uint8_t tin, uint32_t args[4])
{
	return ul_syscall_router(tin, args[0], args[1], args[2], args[3]);
}

void ul_kernel_trap_fault(uint8_t trap_class, uint8_t tin)
{
	ul_printk("TRAP class=%u tin=%u\n", (unsigned)trap_class, (unsigned)tin);
	for (;;)
		;
}

/* =========================================================================
 * Root thread bootstrap
 *
 * root_thread_entry() is the first function executed in the root thread
 * context after the initial context switch.  It calls ul_root_thread()
 * and, if that returns, switches back to the kernel idle context.
 * ========================================================================= */

/*
 * Explicit .bss.* sections: tricore-elf-gcc with -fdata-sections uses the
 * variable name as the section name (e.g. .idle_ctx_g) which is NOT captured
 * by the *(.bss*) linker rule.  Forcing the .bss. prefix fixes placement.
 */
static ul_arch_ctx_t idle_ctx_g
	__attribute__((section(".bss.idle_ctx_g")));
static ul_arch_ctx_t root_ctx_g
	__attribute__((section(".bss.root_ctx_g")));
static uint8_t root_stack_g[4096]
	__attribute__((aligned(8), section(".bss.root_stack_g")));
static const ul_boot_info_t *boot_info_g
	__attribute__((section(".bss.boot_info_g")));

/*
 * Entry point for the root thread context.
 * Runs after ul_arch_ctx_switch(idle_ctx, root_ctx) in ul_kernel_main.
 */
static void root_thread_entry(void *arg)
{
	(void)arg;
	ul_printk("ulipeMicroKernel: root thread\n");
	ul_root_thread(boot_info_g);
	/*
	 * If ul_root_thread() returns (only in test stubs), switch back to
	 * the kernel idle context so the test can verify that path too.
	 */
	ul_arch_ctx_switch(&root_ctx_g, &idle_ctx_g);
	for (;;)
		ul_arch_cpu_idle();
}

/* =========================================================================
 * Kernel main — does not return
 * ========================================================================= */

void ul_kernel_main(const ul_boot_info_t *info)
{
	boot_info_g = info;

	ul_printk("ulipeMicroKernel: kernel entry\n");

	ul_sched_init();
	UL_LOG_DBG("sched init done");

	ul_irq_table_init();
	UL_LOG_DBG("irq table init done");

	ul_arch_ctx_init(&root_ctx_g,
			 root_thread_entry,
			 NULL,
			 (uintptr_t)(root_stack_g + sizeof(root_stack_g)),
			 UL_PRIV_KERNEL);

	ul_printk("ulipeMicroKernel: switching to root thread\n");

	/*
	 * First context switch: idle_ctx_g is initialised in-place by
	 * ul_arch_ctx_switch as it saves the current (kernel-main) PCXI.
	 * When root_thread_entry switches back, execution resumes here.
	 */
	ul_arch_ctx_switch(&idle_ctx_g, &root_ctx_g);

	ul_printk("ulipeMicroKernel: idle loop\n");
	for (;;)
		ul_arch_cpu_idle();
}
