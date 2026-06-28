/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal thread management types and interface.
 * Not part of the public API — do not include from userspace.
 */

#ifndef UL_THREAD_INTERNAL_H
#define UL_THREAD_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <ul/microkernel.h>
#include <ul_arch.h>

#define UL_THREAD_STATE_DEAD      0
#define UL_THREAD_STATE_READY     1
#define UL_THREAD_STATE_RUNNING   2
#define UL_THREAD_STATE_BLOCKED   3
#define UL_THREAD_STATE_SUSPENDED 4

typedef struct ul_thread {
	ul_arch_ctx_t   ctx;
	uint8_t        *stack_base;
	size_t          stack_size;
	uint8_t         priority;
	uint8_t         state;
	ul_privilege_t  privilege;
	ul_tid_t        tid;
	struct ul_thread *next;		/* run-queue linkage */
	struct ul_thread *sleep_next;	/* sleep-queue linkage */
	uint64_t          sleep_until;	/* absolute µs deadline (0 = not sleeping) */
} ul_thread_t;

int          ul_thread_init(ul_thread_t *th, const ul_thread_attr_t *attr,
			    void *stack);
ul_thread_t *ul_thread_by_tid(ul_tid_t tid);
void         ul_thread_set_state(ul_thread_t *th, uint8_t state);

#endif /* UL_THREAD_INTERNAL_H */
