/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IPC endpoint management.
 */

#ifndef UL_EP_INTERNAL_H
#define UL_EP_INTERNAL_H

#include <ul/microkernel.h>
#include <kernel/include/ul_thread_internal.h>

typedef struct ul_endpoint {
	ul_ep_t      id;
	ul_thread_t *receiver;		/* thread blocked in ul_ep_recv() */
	ul_thread_t *caller;		/* thread blocked in ul_ep_call() */
	ul_msg_t     pending_msg;
} ul_endpoint_t;

int           ul_ep_init(ul_endpoint_t *ep, ul_ep_t id);
ul_endpoint_t *ul_ep_by_id(ul_ep_t id);

#endif /* UL_EP_INTERNAL_H */
