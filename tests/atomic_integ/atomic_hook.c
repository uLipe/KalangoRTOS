/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * tests/atomic_integ/atomic_hook.c — kernel-space atomic operation tests
 *
 * ulmk_kernel_pre_root_hook() runs before the scheduler starts, in kernel
 * privilege, with interrupts enabled but no user threads active.
 * This validates that the arch atomic primitives work correctly.
 */

#include <stdint.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_printk.h>

#define PHASE1_ITERS	10000u

void ulmk_kernel_pre_root_hook(void)
{
	volatile uint32_t counter;
	uint32_t old;
	uint32_t i;

	ulmk_printk("atomic_integ: start\n");

	counter = 0u;
	for (i = 0u; i < PHASE1_ITERS; i++)
		ulmk_arch_atomic_add(&counter, 1u);

	if (counter != PHASE1_ITERS) {
		ulmk_printk("atomic_integ: phase1 add FAIL (got %lu)\n",
			  (unsigned long)counter);
		return;
	}

	counter = 42u;
	old = ulmk_arch_atomic_cas(&counter, 42u, 99u);
	if (old != 42u || counter != 99u) {
		ulmk_printk("atomic_integ: phase1 cas-match FAIL\n");
		return;
	}

	old = ulmk_arch_atomic_cas(&counter, 0u, 1u);
	if (counter != 99u) {
		ulmk_printk("atomic_integ: phase1 cas-mismatch FAIL\n");
		return;
	}
	(void)old;

	ulmk_printk("atomic_integ: phase1 PASS\n");
}
