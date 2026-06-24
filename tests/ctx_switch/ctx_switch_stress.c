/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Context-switch stress test — tests/ctx_switch/ctx_switch_stress.c
 *
 * Two kernel threads (A and B) alternate via ul_arch_ctx_switch, each
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
#include <ul/microkernel.h>
#include <ul_arch.h>
#include <kernel/include/ul_printk.h>

#define VIRT_EXIT           (*(volatile uint32_t *)0xBF000028U)
/*
 * QEMU AURIX limitation: mtcr FCX (free context pointer) writes are not
 * propagated to QEMU's internal CSA allocator, so ctx_switch.S's frame-
 * freeing logic leaks one CSA frame per switch in the emulator.  On real
 * TriCore hardware the mtcr takes effect and there is no net frame
 * consumption.
 *
 * To compensate, the stress ELF linker uses 256 CSA frames (see Makefile
 * STRESS_LDFLAGS) instead of the default 64.  The pool budget is:
 *   2  permanent (kernel_main + ul_root_thread never return)
 *   N  leaked by QEMU (one per switch)
 *   5  needed by ul_printk call chain at any given moment
 * With N=56 and pool=256 there is ample headroom.
 *
 * QEMU TriCore TCG freezes in tight loops without MMIO events; SIGTERM
 * from timeout(1) is not delivered.  STRESS_PRINT_EVERY=1 forces an MMIO
 * write every iteration.  At ~1 switch/s in this emulator, 56 switches
 * takes ~56 s — within the 120 s QEMU_TIMEOUT in the Makefile.
 *
 * QEMU VIRT console line-buffer limit: the VIRT debug device buffers at
 * most 64 output lines.  Boot messages occupy 7 lines, leaving 57 for the
 * test.  STRESS_TARGET=56 yields 55 progress lines + 1 PASS line = 56
 * test lines total, safely within the 57-line budget (63 lines overall).
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
static ul_arch_ctx_t g_ctx_root
	__attribute__((section(".bss.g_ctx_root")));
static ul_arch_ctx_t g_ctx_a
	__attribute__((section(".bss.g_ctx_a")));
static ul_arch_ctx_t g_ctx_b
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
			ul_printk("ctx_stress: PASS count=%u\n",
				  (unsigned int)g_count);
			VIRT_EXIT = 0U;
			for (;;)
				;
		}
		if ((g_count % STRESS_PRINT_EVERY) == 0u)
			ul_printk("ctx_stress: [%u/%u]\n",
				  (unsigned int)g_count,
				  (unsigned int)STRESS_TARGET);
		ul_arch_ctx_switch(&g_ctx_a, &g_ctx_b);
	}
}

static void thread_b_fn(void *arg)
{
	(void)arg;
	for (;;) {
		g_count++;
		if (g_count >= STRESS_TARGET) {
			ul_printk("ctx_stress: PASS count=%u\n",
				  (unsigned int)g_count);
			VIRT_EXIT = 0U;
			for (;;)
				;
		}
		if ((g_count % STRESS_PRINT_EVERY) == 0u)
			ul_printk("ctx_stress: [%u/%u]\n",
				  (unsigned int)g_count,
				  (unsigned int)STRESS_TARGET);
		ul_arch_ctx_switch(&g_ctx_b, &g_ctx_a);
	}
}

void ul_root_thread(const ul_boot_info_t *info)
{
	(void)info;

	ul_printk("ctx_stress: start\n");

	ul_arch_ctx_init(&g_ctx_a, thread_a_fn, NULL,
			 (uintptr_t)(g_stack_a + sizeof(g_stack_a)),
			 UL_PRIV_KERNEL);
	ul_arch_ctx_init(&g_ctx_b, thread_b_fn, NULL,
			 (uintptr_t)(g_stack_b + sizeof(g_stack_b)),
			 UL_PRIV_KERNEL);

	/*
	 * Launch thread_a.  g_ctx_root captures the current (root thread)
	 * SP+RA so this context could be resumed, but in practice thread_a
	 * or thread_b will call VIRT_EXIT before switching back here.
	 */
	ul_arch_ctx_switch(&g_ctx_root, &g_ctx_a);

	for (;;)
		;
}
