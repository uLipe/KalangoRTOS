/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal timeout helpers for sleep / IPC wait — not public API.
 */

#ifndef UL_TIMEOUT_INTERNAL_H
#define UL_TIMEOUT_INTERNAL_H

#include <stdint.h>
#include <kernel/include/ulmk_thread_internal.h>

uint32_t ulmk_ms_to_ticks(uint32_t ms);
int      ulmk_timeout_arm(ulmk_thread_t *th, uint32_t ms,
			  void (*cb)(struct ulmk_timeout *to));
void     ulmk_timeout_disarm(ulmk_thread_t *th);

#endif /* UL_TIMEOUT_INTERNAL_H */
