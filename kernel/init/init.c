/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Common C runtime bring-up — kernel/init/init.c
 *
 * The arch startup (startup.S) performs only the CPU-mandatory prologue with
 * interrupts disabled — stack pointer, and on TriCore the CSA free list and
 * small-data anchors — then jumps here.  This file provides the C runtime
 * environment shared by every port: relocate initialized data, clear BSS, run
 * the optional board hook, then hand off to the arch and kernel entry points.
 *
 * Runtime constraints — this code runs before .data/.bss are ready:
 *   - must not read any initialized global or assume BSS is zero;
 *   - uses only linker-provided symbols (link-time constants) and writes RAM.
 * Compiled with -fno-tree-loop-distribute-patterns so the copy/zero loops are
 * never lowered to memcpy/memset calls (see kernel CMake / integ_kernel.mk).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>

/* Linker-provided section boundaries (arch-independent, Layer-1 fragments). */
extern uint32_t __ulmk_kernel_data_load[];
extern uint32_t _ulmk_kernel_data_start[];
extern uint32_t _ulmk_kernel_data_end[];
extern uint32_t _ulmk_kernel_bss_start[];
extern uint32_t _ulmk_kernel_bss_end[];
extern uint32_t __ulmk_user_data_load[];
extern uint32_t _ulmk_user_data_start[];
extern uint32_t _ulmk_user_data_end[];
extern uint32_t _ulmk_user_pool_start[];

static void copy_words(uint32_t *dst, const uint32_t *end, const uint32_t *src)
{
	while (dst < end)
		*dst++ = *src++;
}

static void zero_words(uint32_t *dst, const uint32_t *end)
{
	while (dst < end)
		*dst++ = 0u;
}

/*
 * Common entry from startup.S.  Runs on the kernel stack with interrupts
 * disabled; does not return.
 */
void ulmk_kern_start(void)
{
	ulmk_boot_info_t boot_info = {0};

	/*
	 * Board hook runs first, before .data copy — it may only touch raw
	 * hardware (PLL, wait states, external RAM); no kernel API, no
	 * initialized globals.  Weak no-op when the board provides none.
	 */
	ulmk_board_init();

	copy_words(_ulmk_kernel_data_start, _ulmk_kernel_data_end,
		   __ulmk_kernel_data_load);
	zero_words(_ulmk_kernel_bss_start, _ulmk_kernel_bss_end);

	copy_words(_ulmk_user_data_start, _ulmk_user_data_end,
		   __ulmk_user_data_load);
	zero_words(_ulmk_user_data_end, _ulmk_user_pool_start);

	/* Arch fills boot_info (memory map, tick rate, CSA pool), then hands
	 * the platform-independent kernel the description of the machine. */
	ulmk_arch_init(&boot_info);
	ulmk_kern_main(&boot_info);

	for (;;) {
	}
}
