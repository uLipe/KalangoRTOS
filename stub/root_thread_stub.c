/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Weak stub for ul_root_thread().
 * Linked when no application provides an actual root thread.
 * Causes a deliberate halt so the developer cannot silently miss the entry
 * point (the weak definition still generates a link error if the loader
 * tries to call it with -Werror=missing-declarations on the user side).
 * Full specification: docs/build_system_spec.md §5 and docs/api_spec.md §5
 */

#include <ul/microkernel.h>

__attribute__((weak, noreturn)) void ul_root_thread(const ul_boot_info_t *info)
{
	(void)info;
	/* No application registered a root thread: spin. */
	for (;;)
		;
}
