/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	board_services_init(info);
	sdk_puts("ctx_switch: ROOT THREAD RUNNING\n");
	sdk_puts("ctx_switch: PASS\n");
	ulmk_thread_exit();
}
