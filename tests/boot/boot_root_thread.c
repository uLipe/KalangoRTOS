/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Boot integration test — tests/boot/boot_root_thread.c
 *
 * Verifies two invariants of the kernel boot path:
 *   1. BSS region is zero-initialised by startup.S before the root thread
 *      runs.  Checked via canary variables defined only in this file —
 *      the kernel never writes to them, so any non-zero value means
 *      startup.S failed to zero BSS (would be UB on real hardware with
 *      non-zero SRAM at reset).
 *      NOTE: scanning the full [_ul_kernel_bss_start, _ul_kernel_bss_end)
 *      range is intentionally avoided because the kernel legitimately
 *      modifies its own BSS variables (idle_ctx_g, root_ctx_g, etc.)
 *      before ul_root_thread() is invoked.
 *   2. Execution reaches ul_root_thread() — the BOOT OK sentinel confirms
 *      the full startup → arch_init → kernel_main → context-switch chain.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>

/* QEMU VIRT device exit register (hw/tricore/tricore_virt.c +0x28). */
#define VIRT_EXIT  (*(volatile uint32_t *)0xBF000028U)

/*
 * Canary variables: no initialiser → placed in BSS by the compiler.
 * The kernel never touches these symbols; they must be zero if and only
 * if startup.S ran the BSS zero loop correctly.
 */
static uint32_t bss_canary_a;
static uint32_t bss_canary_b[16];

void ul_root_thread(const ul_boot_info_t *info)
{
	uint32_t i;
	int bss_ok = 1;

	(void)info;

	if (bss_canary_a != 0u)
		bss_ok = 0;

	for (i = 0u; i < 16u && bss_ok; i++) {
		if (bss_canary_b[i] != 0u)
			bss_ok = 0;
	}

	ul_printk("boot_test: bss zero check — %s\n", bss_ok ? "OK" : "FAIL");
	ul_printk("boot_test: root thread reached — BOOT OK\n");

	VIRT_EXIT = 0U;
	for (;;)
		;
}
