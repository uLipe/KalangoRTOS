/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board timer public API — boards/qemu_tc3xx/board_timer.h
 *
 * Provides microsecond sleep via an IPC-backed service thread that owns
 * the STM0 compare-match interrupt.  Callers use board_timer_sleep_us()
 * from any thread with DRIVER privilege (IPC only; no direct MMIO).
 *
 * Usage:
 *   board_timer_start(info);  -- once from board_services_init()
 *   board_timer_sleep_us(10000);
 */

#ifndef BOARD_TIMER_H
#define BOARD_TIMER_H

#include <stdint.h>
#include <ulmk/microkernel.h>

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info);

void board_timer_sleep_us(uint32_t us);

#endif /* BOARD_TIMER_H */
