/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Thread lifecycle handlers — kernel/thread/thread.c
 * Implements: syscall_router.h ul_kern_thread_* prototypes
 * Reference: docs/api_spec.md §6, docs/microkernel_book_tricore.md §9
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

uint32_t ul_kern_thread_self(void)
{
	ul_thread_t *t = ul_sched_current();
	return t ? (uint32_t)t->tid : (uint32_t)(int32_t)UL_TID_INVALID;
}

uint32_t ul_kern_yield(void)
{
	/* TODO: trigger scheduler to pick next ready thread */
	return 0;
}

uint32_t ul_kern_exit(void)
{
	/* TODO: mark current thread dead, trigger reschedule */
	for (;;)
		;
}

uint32_t ul_kern_thread_spawn(uint32_t attr_ptr)
{
	const ul_thread_attr_t *attr = (const ul_thread_attr_t *)(uintptr_t)attr_ptr;
	(void)attr;
	/* TODO: allocate stack from user_pool, initialise TCB, enqueue */
	return (uint32_t)(int32_t)UL_TID_INVALID;
}

uint32_t ul_kern_thread_kill(uint32_t tid)
{
	(void)tid;
	/* TODO: remove thread from run queue, free stack and TCB */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_thread_suspend(uint32_t tid)
{
	(void)tid;
	/* TODO: set state SUSPENDED, dequeue */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_thread_resume(uint32_t tid)
{
	(void)tid;
	/* TODO: set state READY, re-enqueue */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_thread_set_prio(uint32_t tid, uint32_t prio)
{
	(void)tid;
	(void)prio;
	/* TODO: update priority, re-sort run queue */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_thread_get_prio(uint32_t tid)
{
	(void)tid;
	/* TODO: return thread priority */
	return (uint32_t)(int32_t)UL_EINVAL;
}
