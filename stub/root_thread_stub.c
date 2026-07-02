/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/root_thread_stub.c — DOCUMENTATION ONLY, not compiled.
 *
 * The application (component or board) is responsible for providing
 * ul_root_thread().  Forgetting it is a link error, which is intentional.
 *
 * Minimum implementation:
 *
 * #include <ul/microkernel.h>
 * void ul_root_thread(const ul_boot_info_t *info)
 * {
 *     (void)info;
 *     ul_thread_exit();
 * }
 */

/*
#include <ul/microkernel.h>

__attribute__((weak, noreturn)) void ul_root_thread(const ul_boot_info_t *info)
{
	(void)info;
	for (;;)
		;
}
*/
