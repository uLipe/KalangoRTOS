/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/root_thread_stub.c — DOCUMENTATION ONLY, not compiled.
 *
 * The application (component or board) is responsible for providing
 * ulmk_root_thread().  Forgetting it is a link error, which is intentional.
 *
 * Minimum implementation:
 *
 * #include <ulmk/microkernel.h>
 * void ulmk_root_thread(const ulmk_boot_info_t *info)
 * {
 *     (void)info;
 *     ulmk_thread_exit();
 * }
 */

/*
#include <ulmk/microkernel.h>

__attribute__((weak, noreturn)) void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;
	for (;;)
		;
}
*/
