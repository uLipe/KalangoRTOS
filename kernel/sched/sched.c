/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Scheduler core — kernel/sched/sched.c
 * Dispatches through ul_sched_class_t vtable; manages current/idle state.
 */

#include <stddef.h>
#include <ul/config.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_thread_internal.h>
#include <ul_arch.h>

static const ul_sched_class_t	*sched_class;
static ul_arch_ctx_t		*sched_idle;
static ul_thread_t		*sched_current;

void ul_sched_init(ul_arch_ctx_t *idle)
{
	sched_idle    = idle;
	sched_current = NULL;
	sched_class   = NULL;
}

void ul_sched_set_class(const ul_sched_class_t *cls)
{
	sched_class = cls;
	cls->init();
}

/*
 * ul_sched_start — perform the first context switch from idle to a thread.
 *
 * Records first as current, configures MPU for the first thread, then
 * switches the idle context (kernel_main's stack) to the thread.
 */
void ul_sched_start(ul_arch_ctx_t *idle, ul_thread_t *first)
{
	sched_idle           = idle;
	sched_current        = first;
	first->state         = UL_THREAD_STATE_RUNNING;
	ul_arch_mpu_switch(first->regions, first->region_count, 1u);
	ul_arch_ctx_switch(idle, &first->ctx);
}

/*
 * ul_sched_schedule — switch to the highest-priority ready thread.
 *
 * If the run queue is empty, switches to the idle context.  If pick_next
 * returns the same thread that is already running (only one ready thread
 * of that priority after a yield), the switch is still performed so the
 * CSA chain unwinds correctly through ul_arch_ctx_switch.
 */
void ul_sched_schedule(void)
{
	ul_thread_t   *prev = sched_current;
	ul_thread_t   *next = sched_class->pick_next();
	ul_arch_ctx_t *from;
	ul_arch_ctx_t *to;

	from = prev ? &prev->ctx : sched_idle;

	if (next) {
		sched_current = next;
		next->state   = UL_THREAD_STATE_RUNNING;
		to = &next->ctx;
		ul_arch_mpu_switch(next->regions, next->region_count, 1u);
	} else {
		sched_current = NULL;
		to = sched_idle;
		ul_arch_mpu_switch(NULL, 0u, 1u);
	}

	if (from == to)
		return;

	ul_arch_ctx_switch(from, to);
}

void ul_sched_enqueue(ul_thread_t *t)
{
	sched_class->enqueue(t);
}

void ul_sched_dequeue(ul_thread_t *t)
{
	sched_class->dequeue(t);
}

ul_thread_t *ul_sched_current(void)
{
	return sched_current;
}

ul_thread_t *ul_sched_pick_next(void)
{
	return sched_class->pick_next();
}

void ul_sched_tick(void)
{
	/* No time-slice accounting yet — added when sleep is implemented. */
}
