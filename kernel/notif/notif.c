/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Notification handlers — kernel/notif/notif.c
 * Implements: syscall_router.h ul_kern_notif_* prototypes
 * Reference: docs/api_spec.md §8, docs/microkernel_book_tricore.md §9
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_notif_internal.h>
#include <kernel/include/ul_sched.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

uint32_t ul_kern_notif_create(void)
{
	/* TODO: allocate from static pool */
	return (uint32_t)(int32_t)UL_NOTIF_INVALID;
}

uint32_t ul_kern_notif_signal(uint32_t notif, uint32_t bits)
{
	(void)notif;
	(void)bits;
	/*
	 * TODO:
	 *   1. Find notif object
	 *   2. Atomically OR bits into notif->bits
	 *   3. If waiter != NULL && (notif->bits & waiter->wait_mask):
	 *        wake waiter, clear matched bits
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_notif_wait(uint32_t notif, uint32_t mask, uint32_t bits_ptr)
{
	uint32_t *out = (uint32_t *)(uintptr_t)bits_ptr;
	(void)notif;
	(void)mask;
	(void)out;
	/*
	 * TODO:
	 *   1. If (notif->bits & mask): consume bits, write to *out, return 0
	 *   2. Else: block caller, set waiter and wait_mask
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_notif_poll(uint32_t notif, uint32_t mask)
{
	(void)notif;
	(void)mask;
	/* TODO: non-blocking read of notif->bits & mask */
	return 0;
}
