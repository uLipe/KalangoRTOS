/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/board_init_stub.c
 *
 * No-op ulmk_board_init() for integration test builds.
 * Included directly by test Makefiles that target QEMU (no early HW setup
 * needed).  NOT compiled into the CMake kernel build — the board provides
 * ulmk_board_init() there (see boards/<board>/board_services.c).
 *
 * Called from ulmk_kern_start() before .data copy; no globals, no kernel API.
 */

void ulmk_board_init(void)
{
}
