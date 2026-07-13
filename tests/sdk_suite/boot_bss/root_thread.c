/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include "sdk_test_util.h"

static uint32_t bss_canary_a;
static uint32_t bss_canary_b[16];

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	uint32_t i;
	int      bss_ok = 1;

	board_services_init(info);

	if (bss_canary_a != 0u)
		bss_ok = 0;
	for (i = 0u; i < 16u && bss_ok; i++) {
		if (bss_canary_b[i] != 0u)
			bss_ok = 0;
	}

	sdk_puts(bss_ok ? "boot_bss: bss zero check — OK\n"
			: "boot_bss: bss zero check — FAIL\n");
	sdk_puts("boot_bss: root thread reached — BOOT OK\n");
	if (bss_ok)
		sdk_puts("boot_bss: PASS\n");
	else
		sdk_puts("boot_bss: FAIL\n");
	ulmk_thread_exit();
}
