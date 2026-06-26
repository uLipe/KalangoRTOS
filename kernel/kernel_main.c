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
#include <kernel/include/ul_thread_internal.h>
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
 * ========================================================================= */

/*
 * Explicit .bss.* sections: tricore-elf-gcc with -fdata-sections uses the
 * variable name as the section name which is NOT captured by *(.bss*).
 * Forcing the .bss. prefix fixes placement.
 */
static ul_arch_ctx_t  idle_ctx_g
	__attribute__((section(".bss.idle_ctx_g")));
static ul_thread_t    root_thread_g
	__attribute__((section(".bss.root_thread_g")));
static uint8_t        root_stack_g[4096]
	__attribute__((aligned(8), section(".bss.root_stack_g")));

static void root_thread_entry(void *arg)
{
	ul_printk("ulipeMicroKernel: root thread\n");
	ul_root_thread((const ul_boot_info_t *)arg);
	/*
	 * ul_root_thread() should not return.  If it does (stub/test path),
	 * exit cleanly via the scheduler.
	 */
	ul_thread_exit();
}

/* =========================================================================
 * Kernel main — does not return
 * ========================================================================= */

void ul_kernel_main(const ul_boot_info_t *info)
{
	ul_thread_attr_t root_attr;

	ul_printk("ulipeMicroKernel: kernel entry\n");

	ul_sched_init(&idle_ctx_g);
	ul_sched_set_class(&ul_fifo_rt_class);
	UL_LOG_DBG("sched init done");

	ul_irq_table_init();
	UL_LOG_DBG("irq table init done");

	root_attr.name      = "root";
	root_attr.entry     = root_thread_entry;
	root_attr.arg       = (void *)info;
	root_attr.priority  = 0;
	root_attr.stack_size = sizeof(root_stack_g);
	root_attr.privilege = UL_PRIV_KERNEL;

	ul_thread_init(&root_thread_g, &root_attr, root_stack_g);

	ul_printk("ulipeMicroKernel: switching to root thread\n");

	/*
	 * ul_sched_start saves kernel_main's context into idle_ctx_g and
	 * switches to root_thread_g.  Returns here only when the run queue
	 * is empty and the scheduler switches back to idle.
	 */
	ul_sched_start(&idle_ctx_g, &root_thread_g);

	ul_printk("ulipeMicroKernel: idle loop\n");
	for (;;)
		ul_arch_cpu_idle();
}
