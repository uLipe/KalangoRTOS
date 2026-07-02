/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * stub/board_init_stub.c
 *
 * No-op ul_board_init() for integration test builds.
 * Included directly by test Makefiles that target QEMU (no early HW setup
 * needed).  NOT compiled into the CMake kernel build — the board provides
 * ul_board_init() there (see boards/<board>/board_services.c).
 *
 * Called from startup.S before .data copy; no globals, no kernel API.
 */

void ul_board_init(void)
{
}
