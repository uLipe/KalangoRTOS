/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_ep_t  ep;
	ulmk_msg_t msg;
	int        rc;

	board_services_init(info);
	sdk_puts("ipc_timeout: start\n");

	ep = ulmk_ep_create();
	if (ep == ULMK_EP_INVALID) {
		sdk_puts("ipc_timeout: FAIL create\n");
		ulmk_thread_exit();
	}

	msg.label    = 1u;
	msg.words[0] = 0u;
	rc = ulmk_ep_call_timeout(ep, &msg, 20u);
	if (rc != ULMK_ETIMEOUT) {
		sdk_puts("ipc_timeout: FAIL rc=\n");
		sdk_put_u32((uint32_t)rc);
		sdk_puts("\n");
		ulmk_thread_exit();
	}

	sdk_puts("ipc_timeout: PASS\n");
	ulmk_thread_exit();
}
