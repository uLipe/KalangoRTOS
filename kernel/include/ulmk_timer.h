/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal timing wheel — kernel/include/ulmk_timer.h
 *
 * Non-cascading hierarchical wheel (Linux 4.8 style).  Arm/cancel/advance
 * are O(1) except for O(k) expiry callbacks on the current bucket.
 * Not part of the public API.
 */

#ifndef UL_TIMER_H
#define UL_TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/include/list.h>

/* 5 levels × 64 buckets, gran shift 3 (8×).  At 1 kHz ≈ 4.3 min max. */
#define ULMK_TIMER_LVL_BITS		6u
#define ULMK_TIMER_LVL_SIZE		(1u << ULMK_TIMER_LVL_BITS)
#define ULMK_TIMER_LVL_MASK		(ULMK_TIMER_LVL_SIZE - 1u)
#define ULMK_TIMER_LVL_CLK_SHIFT	3u
#define ULMK_TIMER_LVL_DEPTH		5u

#define ULMK_TIMER_LVL_SHIFT(n)	((n) * ULMK_TIMER_LVL_CLK_SHIFT)
#define ULMK_TIMER_LVL_GRAN(n)	(1u << ULMK_TIMER_LVL_SHIFT(n))

/*
 * Capacity of the outer level: ((64-1) << (4*3)) = 63 << 12 = 258048 ticks.
 * At 1 kHz ≈ 258 s (~4.3 min).
 */
#define ULMK_TIMER_TIMEOUT_MAX \
	(((ULMK_TIMER_LVL_SIZE - 1u) << \
	  ULMK_TIMER_LVL_SHIFT(ULMK_TIMER_LVL_DEPTH - 1u)))

struct ulmk_timeout {
	sys_dnode_t node;
	uint64_t    expires;
	void      (*cb)(struct ulmk_timeout *to);
};

void     ulmk_timer_init(void);
int      ulmk_timer_add(struct ulmk_timeout *to, uint32_t delta_ticks);
bool     ulmk_timer_cancel(struct ulmk_timeout *to);
void     ulmk_timer_run(void);
uint64_t ulmk_timer_jiffies(void);

/* Test / arch tick path: advance local clock by one tick then run. */
void     ulmk_timer_tick(void);

#endif /* UL_TIMER_H */
