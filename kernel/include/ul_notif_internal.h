/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal notification object management.
 */

#ifndef UL_NOTIF_INTERNAL_H
#define UL_NOTIF_INTERNAL_H

#include <stdint.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_thread_internal.h>

typedef struct ul_notif_obj {
	ul_notif_t   id;
	uint32_t     bits;
	ul_thread_t *waiter;		/* thread blocked in ul_notif_wait() */
	uint32_t     wait_mask;
} ul_notif_obj_t;

int             ul_notif_obj_init(ul_notif_obj_t *n, ul_notif_t id);
ul_notif_obj_t *ul_notif_by_id(ul_notif_t id);

#endif /* UL_NOTIF_INTERNAL_H */
