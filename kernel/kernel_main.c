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

/*
 * Walk one PCXI link word to the CSA frame address.
 * Only works for SRAM segment 7 (0x7xxx_xxxx), which is all we have.
 */
static inline uint32_t *pcxi_to_csa(uint32_t pcxi)
{
	uint32_t link = pcxi & 0x000FFFFFu;	/* strip PCPN/PIE/UL */

	return (uint32_t *)(((link & 0x70000u) << 12) | ((link & 0xFFFFu) << 6));
}

void ul_kernel_trap_fault(uint8_t trap_class, uint8_t tin)
{
	uint32_t pcxi_here;
	uint32_t fcx_val;
	uint32_t *f_call;
	uint32_t pcxi_trap;
	uint32_t *f_fault;

	__asm__ volatile("mfcr %0, 0xFE00" : "=d"(pcxi_here));
	__asm__ volatile("mfcr %0, 0xFE38" : "=d"(fcx_val));

	ul_printk("TRAP class=%u tin=%u\n", (unsigned)trap_class, (unsigned)tin);
	ul_printk("  PCXI_here=%x FCX=%x\n", (unsigned)pcxi_here, (unsigned)fcx_val);

	/*
	 * UC1: upper context saved by "call ul_kernel_trap_fault" in vectors.S.
	 * UC0: upper context saved by the hardware trap mechanism itself.
	 *
	 * Per TC1.6.1 spec, hardware sets A11 = faulting PC before entering the
	 * trap vector.  Dump all 16 words of both frames for full diagnosis.
	 */
	f_call = pcxi_to_csa(pcxi_here);
	pcxi_trap = f_call[0];

	{
		uint32_t w;

		ul_printk("  UC1 at %p (call frame):\n", (void *)f_call);
		for (w = 0u; w < 16u; w++)
			ul_printk("    [%u]=%x\n", (unsigned)w,
				  (unsigned)f_call[w]);
	}

	f_fault = pcxi_to_csa(pcxi_trap);

	{
		uint32_t w;

		ul_printk("  UC0 at %p (hw-trap frame):\n", (void *)f_fault);
		for (w = 0u; w < 16u; w++)
			ul_printk("    [%u]=%x\n", (unsigned)w,
				  (unsigned)f_fault[w]);
	}

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
