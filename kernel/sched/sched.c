/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Fixed-priority round-robin scheduler — kernel/sched/sched.c
 * Called by: ul_arch_ctx_switch() hook and ul_kernel_tick()
 */

#include <stddef.h>
#include <ul/config.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_thread_internal.h>
#include <ul_arch.h>

void ul_sched_init(void)
{
	/* TODO */
}

void ul_sched_enqueue(ul_thread_t *th)
{
	(void)th;
	/* TODO */
}

void ul_sched_dequeue(ul_thread_t *th)
{
	(void)th;
	/* TODO */
}

ul_thread_t *ul_sched_pick_next(void)
{
	return NULL; /* TODO */
}

ul_thread_t *ul_sched_current(void)
{
	return NULL; /* TODO */
}

void ul_sched_tick(void)
{
	/* TODO: time-slice accounting, ul_kernel_tick() calls this */
}
