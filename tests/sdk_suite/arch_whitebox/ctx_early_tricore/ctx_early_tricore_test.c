/* SPDX-License-Identifier: MIT */
/*
 * TriCore CSA early context fabrication — userspace sentinel.
 * Core path runs in ctx_early_tricore_kernel.c before MPU enable.
 */

#include <stdint.h>
#include "../../../test_support.h"
#include <ulmk/microkernel.h>

extern int csa_ctx_early_passed(void);

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;

	ulmk_printk("ctx_early_tricore: start\n");

	if (csa_ctx_early_passed()) {
		ulmk_printk("ctx_early_tricore: PASS\n");
		ulmk_sim_exit(0);
	}

	ulmk_printk("ctx_early_tricore: FAIL\n");
	ulmk_sim_exit(1);
}
