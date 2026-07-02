/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal notification object management.
 */

#ifndef UL_NOTIF_INTERNAL_H
#define UL_NOTIF_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_thread_internal.h>

typedef struct ulmk_notif_obj {
	ulmk_notif_t   id;
	bool         active;
	uint32_t     bits;
	ulmk_thread_t *waiter;		/* thread blocked in ulmk_notif_wait() */
	uint32_t     wait_mask;
} ulmk_notif_obj_t;

int             ulmk_notif_obj_init(ulmk_notif_obj_t *n, ulmk_notif_t id);
ulmk_notif_obj_t *ulmk_notif_by_id(ulmk_notif_t id);

/*
 * Core logic with native pointer types — used directly by unit tests.
 */
int      notif_signal_impl(ulmk_notif_t id, uint32_t bits);
int      notif_wait_impl(ulmk_notif_t id, uint32_t mask, uint32_t *out);
uint32_t notif_poll_impl(ulmk_notif_t id, uint32_t mask);

#endif /* UL_NOTIF_INTERNAL_H */
