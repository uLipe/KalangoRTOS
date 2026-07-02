/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board services entry point — boards/qemu_tc3xx/board_services.h
 *
 * board_services_init() starts all board hardware services (console, etc.)
 * and returns with every service endpoint ready.  It must be called from
 * the root thread before any component tries to use a board service.
 *
 * A weak no-op stub is provided in stub/board_services_stub.c so that
 * kernel-only builds link cleanly without this board's sources.
 */

#ifndef BOARD_SERVICES_H
#define BOARD_SERVICES_H

#include <ul/microkernel.h>

void board_services_init(const ul_boot_info_t *info);

#endif /* BOARD_SERVICES_H */
