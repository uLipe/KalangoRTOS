/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Physical bump allocator — kernel/mem/phys_alloc.c
 *
 * Carves aligned blocks from a contiguous RAM region defined at boot.
 * Memory is never freed (allocations are permanent until reboot).
 * Used for thread stacks allocated by ul_kern_thread_spawn.
 *
 * Backing region is the .user_pool section:
 *   [_ul_user_pool_start, _ul_user_pool_end)
 */

#include <stddef.h>
#include <stdint.h>
#include <kernel/include/ul_mem_internal.h>

#define ALIGN_8(x)	(((x) + 7u) & ~(size_t)7u)

static uint8_t	*pool_cursor;
static uint8_t	*pool_end;

void ul_phys_alloc_init(uintptr_t base, uintptr_t end)
{
	pool_cursor = (uint8_t *)base;
	pool_end    = (uint8_t *)end;
}

/*
 * Returns an 8-byte-aligned block of @size bytes, or NULL if the pool is
 * exhausted.  The returned block is NOT zeroed.
 */
void *ul_phys_alloc(size_t size)
{
	size_t	 aligned = ALIGN_8(size);
	uint8_t	*p;

	if (!pool_cursor || pool_cursor + aligned > pool_end)
		return NULL;

	p = pool_cursor;
	pool_cursor += aligned;
	return p;
}
