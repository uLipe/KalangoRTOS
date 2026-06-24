/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Weak no-op for ul_board_init().
 * Linked when no board-specific file provides a strong override.
 * Full specification: docs/arch_api_spec.md §11.1
 */

__attribute__((weak)) void ul_board_init(void)
{
	/* intentionally empty */
}
