/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_notif_t n;
	uint32_t     bits;
	int          rc;

	board_services_init(info);
	sdk_puts("notif_timeout: start\n");

	n = ulmk_notif_create();
	if (n == ULMK_NOTIF_INVALID) {
		sdk_puts("notif_timeout: FAIL create\n");
		ulmk_thread_exit();
	}

	bits = 0u;
	rc = ulmk_notif_wait_timeout(n, 1u, &bits, 20u);
	if (rc != ULMK_ETIMEOUT) {
		sdk_puts("notif_timeout: FAIL rc=\n");
		sdk_put_u32((uint32_t)rc);
		sdk_puts("\n");
		ulmk_thread_exit();
	}

	sdk_puts("notif_timeout: PASS\n");
	ulmk_thread_exit();
}
