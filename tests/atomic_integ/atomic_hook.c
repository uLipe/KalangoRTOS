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
#include "../test_support.h"

#define PHASE1_ITERS	10000u

void atomic_integ_phase1(void)
{
	volatile uint32_t counter;
	uint32_t old;
	uint32_t i;

	ulmk_printk("atomic_integ: start\n");

	/* Verify atomic_add accumulates correctly */
	counter = 0u;
	for (i = 0u; i < PHASE1_ITERS; i++)
		ulmk_arch_atomic_add(&counter, 1u);

	if (counter != PHASE1_ITERS) {
		ulmk_printk("atomic_integ: phase1 add FAIL (got %lu)\n",
			  (unsigned long)counter);
		ulmk_sim_exit(1);
	}

	/* Verify atomic_cas succeeds on match */
	counter = 42u;
	old = ulmk_arch_atomic_cas(&counter, 42u, 99u);
	if (old != 42u || counter != 99u) {
		ulmk_printk("atomic_integ: phase1 cas-match FAIL\n");
		ulmk_sim_exit(1);
	}

	/* Verify atomic_cas does not modify on mismatch */
	old = ulmk_arch_atomic_cas(&counter, 0u, 1u);
	if (counter != 99u) {
		ulmk_printk("atomic_integ: phase1 cas-mismatch FAIL\n");
		ulmk_sim_exit(1);
	}
	(void)old;

	ulmk_printk("atomic_integ: phase1 PASS\n");
}
