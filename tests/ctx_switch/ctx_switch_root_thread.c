/* SPDX-License-Identifier: MIT */
/*
 * Context-switch integration test — root thread stub
 * tests/ctx_switch/ctx_switch_root_thread.c
 *
 * Validates that ulmk_arch_ctx_switch() correctly launches ulmk_root_thread()
 * in its own context (separate stack, PSW, CSA chain).
 *
 * On success: prints sentinel and exits QEMU cleanly.
 * If the sentinel is missing the Makefile grep reports FAIL.
 */

#include <ulmk/microkernel.h>
#include "../test_support.h"


void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;
	ulmk_printk("ctx_switch_test: ROOT THREAD RUNNING\n");

	/*
	 * Return normally — root_thread_entry() in kernel_main.c will
	 * switch back to the idle context on our behalf.
	 * Then kernel_main prints "idle loop" before spinning on wait.
	 *
	 * For the integration test we exit QEMU here to avoid blocking
	 * on the idle wait instruction (no tick IRQ configured yet).
	 */
	ulmk_sim_exit(0);
}
