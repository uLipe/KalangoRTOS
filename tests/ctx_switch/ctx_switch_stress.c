/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Context-switch stress test — tests/ctx_switch/ctx_switch_stress.c
 *
 * Two kernel threads (A and B) alternate via ulmk_arch_ctx_switch, each
 * incrementing a shared counter on every activation.  The test passes
 * when the counter reaches STRESS_TARGET.
 *
 * If the CSA pool is exhausted (e.g. due to a per-switch frame leak) the
 * CPU silently hangs on an FCU trap.  The Makefile wraps QEMU with
 * `timeout` so that hang → QEMU killed → missing sentinel → FAIL.
 *
 * Sentinels checked by Makefile:
 *   "ctx_stress: start"
 *   "ctx_stress: PASS"
 */

#include <stdint.h>
#include "../test_support.h"
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>

/*
 * Pool budget with the SVLCX/RSLCX/RFE context-switch implementation:
 *   - RSLCX and RFE free CSA frames via hardware, so there is no net
 *     frame leak per switch (unlike the old JI-based approach).
 *   - While a thread is suspended it holds 2 frames (CALL + SVLCX).
 *   - A running thread holds ~1-2 frames for each active call level.
 *   - ulmk_printk call chain is ~5 levels deep.
 *   - Total worst-case: 2 suspended × 2 + 1 running × 7 ≈ 11 frames.
 *   - Default 64-frame pool (boot_test.ld) is sufficient.
 *
 * QEMU TriCore TCG freezes in tight loops without MMIO events; SIGTERM
 * from timeout(1) is not delivered.  STRESS_PRINT_EVERY=1 forces an MMIO
 * write every iteration.  At ~1 switch/s in this emulator, 56 switches
 * takes ~56 s — within the 120 s QEMU_TIMEOUT in the Makefile.
 *
 * Console line-buffer budget: boot messages occupy ~7 lines.
 * STRESS_TARGET=56 yields 55 progress lines + 1 PASS line = 56 test lines.
 */
#define STRESS_TARGET       56u
#define STRESS_PRINT_EVERY  1u

/*
 * Explicit .bss.* sections: tricore-elf-gcc with -fdata-sections generates
 * section names without the ".bss." prefix (e.g. just ".g_count"), which are
 * not captured by *(.bss*) in the linker script and end up in PFLASH (ro).
 * Force placement in SRAM by specifying the section name explicitly.
 */
static volatile uint32_t g_count
	__attribute__((section(".bss.g_count")));
static ulmk_arch_ctx_t g_ctx_root
	__attribute__((section(".bss.g_ctx_root")));
static ulmk_arch_ctx_t g_ctx_a
	__attribute__((section(".bss.g_ctx_a")));
static ulmk_arch_ctx_t g_ctx_b
	__attribute__((section(".bss.g_ctx_b")));
static uint8_t g_stack_a[2048]
	__attribute__((aligned(8), section(".bss.g_stack_a")));
static uint8_t g_stack_b[2048]
	__attribute__((aligned(8), section(".bss.g_stack_b")));

static void thread_b_fn(void *arg);

static void thread_a_fn(void *arg)
{
	(void)arg;
	for (;;) {
		g_count++;
		if (g_count >= STRESS_TARGET) {
			ulmk_printk("ctx_stress: PASS count=%u\n",
				  (unsigned int)g_count);
			ulmk_sim_exit(0);
		}
		if ((g_count % STRESS_PRINT_EVERY) == 0u)
			ulmk_printk("ctx_stress: [%u/%u]\n",
				  (unsigned int)g_count,
				  (unsigned int)STRESS_TARGET);
		ulmk_arch_ctx_switch(&g_ctx_a, &g_ctx_b);
	}
}

static void thread_b_fn(void *arg)
{
	(void)arg;
	for (;;) {
		g_count++;
		if (g_count >= STRESS_TARGET) {
			ulmk_printk("ctx_stress: PASS count=%u\n",
				  (unsigned int)g_count);
			ulmk_sim_exit(0);
		}
		if ((g_count % STRESS_PRINT_EVERY) == 0u)
			ulmk_printk("ctx_stress: [%u/%u]\n",
				  (unsigned int)g_count,
				  (unsigned int)STRESS_TARGET);
		ulmk_arch_ctx_switch(&g_ctx_b, &g_ctx_a);
	}
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;

	ulmk_printk("ctx_stress: start\n");

	ulmk_arch_ctx_init(&g_ctx_a, thread_a_fn, NULL,
			 (uintptr_t)(g_stack_a + sizeof(g_stack_a)),
			 ULMK_PRIV_KERNEL);
	ulmk_arch_ctx_init(&g_ctx_b, thread_b_fn, NULL,
			 (uintptr_t)(g_stack_b + sizeof(g_stack_b)),
			 ULMK_PRIV_KERNEL);

	/*
	 * Launch thread_a.  g_ctx_root captures the current (root thread)
	 * SP+RA so this context could be resumed, but in practice thread_a
	 * or thread_b will call ulmk_sim_exit before switching back here.
	 */
	ulmk_arch_ctx_switch(&g_ctx_root, &g_ctx_a);

	for (;;)
		;
}
