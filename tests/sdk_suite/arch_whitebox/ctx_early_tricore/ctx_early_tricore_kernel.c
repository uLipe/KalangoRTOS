/* SPDX-License-Identifier: MIT */
/*
 * TriCore CSA early context test — kernel-side (runs before MPU enable).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_printk.h>

#define EXPECTED_ARG	((void *)0xC0FFEE42u)

static volatile uintptr_t g_ctx_early_result;

static ulmk_arch_ctx_t g_ctx_root;
static ulmk_arch_ctx_t g_ctx_worker;
static uint8_t g_worker_stack[2048] __attribute__((aligned(8)));

static void worker_fn(void *arg)
{
	g_ctx_early_result = (uintptr_t)arg;
	ulmk_arch_ctx_switch(&g_ctx_worker, &g_ctx_root);
	for (;;)
		;
}

void csa_ctx_run_early(void)
{
	g_ctx_early_result = 0u;

	ulmk_arch_ctx_init(&g_ctx_worker,
			 worker_fn,
			 EXPECTED_ARG,
			 (uintptr_t)(g_worker_stack + sizeof(g_worker_stack)),
			 ULMK_PRIV_KERNEL);

	ulmk_arch_ctx_switch(&g_ctx_root, &g_ctx_worker);

	if (g_ctx_early_result == (uintptr_t)EXPECTED_ARG)
		ulmk_printk("ctx_early_tricore: early arg pass — OK\n");
	else
		ulmk_printk("ctx_early_tricore: early arg pass — FAIL\n");

	ulmk_arch_ctx_free(&g_ctx_worker);
	ulmk_arch_ctx_init(&g_ctx_worker,
			    worker_fn,
			    EXPECTED_ARG,
			    (uintptr_t)(g_worker_stack + sizeof(g_worker_stack)),
			    ULMK_PRIV_KERNEL);
	ulmk_arch_ctx_switch(&g_ctx_root, &g_ctx_worker);
	ulmk_arch_ctx_free(&g_ctx_worker);
}

int csa_ctx_early_passed(void)
{
	return g_ctx_early_result == (uintptr_t)EXPECTED_ARG;
}
