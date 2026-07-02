/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * CSA context fabrication test — tests/csa_ctx/csa_ctx_test.c
 *
 * Tests the CSA-based context switch path end-to-end:
 *   1. ulmk_arch_ctx_init() fabricates a valid two-frame CSA chain.
 *   2. The first switch via RSLCX+RFE starts the new thread at its entry
 *      via _ulmk_thread_trampoline.
 *   3. The trampoline correctly passes arg from D4 to A4 (pointer ABI).
 *   4. The worker thread switches back to the root context (cooperative).
 *   5. The root thread verifies the received arg value.
 *
 * Sentinel checked by Makefile: "csa_ctx_test: PASS"
 */

#include <stdint.h>
#include "../test_support.h"
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_printk.h>


/* Arbitrary non-trivial pointer used as the test argument. */
#define EXPECTED_ARG	((void *)0xC0FFEE42u)

static volatile uintptr_t g_received_arg
	__attribute__((section(".bss.g_received_arg")));

static ulmk_arch_ctx_t g_ctx_root
	__attribute__((section(".bss.g_ctx_root")));
static ulmk_arch_ctx_t g_ctx_worker
	__attribute__((section(".bss.g_ctx_worker")));

static uint8_t g_worker_stack[2048]
	__attribute__((aligned(8), section(".bss.g_worker_stack")));

static void worker_fn(void *arg)
{
	g_received_arg = (uintptr_t)arg;
	ulmk_arch_ctx_switch(&g_ctx_worker, &g_ctx_root);
	/* Should not be reached in this test. */
	for (;;)
		;
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;

	ulmk_printk("csa_ctx_test: start\n");

	ulmk_arch_ctx_init(&g_ctx_worker,
			 worker_fn,
			 EXPECTED_ARG,
			 (uintptr_t)(g_worker_stack + sizeof(g_worker_stack)),
			 ULMK_PRIV_KERNEL);

	ulmk_arch_ctx_switch(&g_ctx_root, &g_ctx_worker);

	/* Resumed after worker switched back. */
	if (g_received_arg == (uintptr_t)EXPECTED_ARG) {
		ulmk_printk("csa_ctx_test: arg pass — OK (got 0x%lx)\n",
			  (unsigned long)g_received_arg);
		ulmk_printk("csa_ctx_test: PASS\n");
		ulmk_sim_exit(0);
	} else {
		ulmk_printk("csa_ctx_test: arg pass — FAIL "
			  "(expected 0x%lx, got 0x%lx)\n",
			  (unsigned long)(uintptr_t)EXPECTED_ARG,
			  (unsigned long)g_received_arg);
		ulmk_printk("csa_ctx_test: FAIL\n");
		ulmk_sim_exit(1);
	}

	for (;;)
		;
}
