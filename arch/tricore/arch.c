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
	return 0; /* TODO: MFCR ICR, disable IE */
}

void ul_arch_cpu_irq_restore(ul_arch_irq_key_t key)
{
	(void)key; /* TODO: MTCR ICR */
}

void ul_arch_cpu_irq_enable(void)
{
	/* TODO: ENABLE instruction */
}

void ul_arch_cpu_irq_disable(void)
{
	/* TODO: DISABLE instruction */
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

void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *arg), void *arg,
		      uintptr_t stack_top, ul_privilege_t priv)
{
	(void)arg;
	(void)priv;
	ctx->sp = (uint32_t)stack_top;
	ctx->ra = (uint32_t)(uintptr_t)entry;
}

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
	(void)btv;
	(void)biv;
	(void)isp_top;
	/* TODO: MTCR BTV, BIV, ISP */
}

void ul_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
	(void)srpn;
	(void)priority;
	(void)cpu_id;
	/* TODO: write SRC[srpn] register */
}

void ul_arch_irq_src_enable(uint8_t srpn)
{
	(void)srpn;
	/* TODO: set SRC[srpn].SRE */
}

void ul_arch_irq_src_disable(uint8_t srpn)
{
	(void)srpn;
	/* TODO: clear SRC[srpn].SRE */
}

void ul_arch_irq_src_ack(uint8_t srpn)
{
	(void)srpn;
	/* TODO: set SRC[srpn].CLRR */
}

bool ul_arch_irq_src_is_pending(uint8_t srpn)
{
	(void)srpn;
	return false; /* TODO: read SRC[srpn].SRR */
}

/* =========================================================================
 * Tick timer
 * ========================================================================= */

void ul_arch_tick_init(uint32_t tick_hz)
{
	(void)tick_hz;
	/* TODO: configure STM compare match for 1/tick_hz period */
}

uint64_t ul_arch_tick_get(void)
{
	return 0; /* TODO: read STM TIM0 + TIM6 */
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

void ul_arch_init(ul_boot_info_t *info)
{
	(void)info;
	/*
	 * TODO:
	 *   1. ul_arch_csa_pool_init(_ul_csa_pool_start, pool_size)
	 *   2. ul_arch_mpu_init()
	 *   3. ul_arch_irq_vectors_init(_ul_trap_table, _ul_int_table,
	 *                               _ul_isr_stack_top)
	 *   4. Fill info->mem[], info->tick_hz, etc.
	 */
}
