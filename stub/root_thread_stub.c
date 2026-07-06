/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Default root thread — linked when no enabled component declares ROOT_THREAD.
 */

#include <ulmk/microkernel.h>

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;
	ulmk_thread_exit();
}
