/* SPDX-License-Identifier: MIT */
/*
 * CSA context fabrication test — userspace sentinel only.
 * Core CSA path is exercised in csa_ctx_kernel.c before MPU enable.
 */

#include <stdint.h>
#include "../test_support.h"
#include <ulmk/microkernel.h>

extern int csa_ctx_early_passed(void);

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;

	ulmk_printk("csa_ctx_test: start\n");

	if (csa_ctx_early_passed()) {
		ulmk_printk("csa_ctx_test: PASS\n");
		ulmk_sim_exit(0);
	}

	ulmk_printk("csa_ctx_test: FAIL\n");
	ulmk_sim_exit(1);
}
