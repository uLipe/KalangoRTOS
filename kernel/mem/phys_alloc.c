/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * First-fit free-list allocator — kernel/mem/phys_alloc.c
 *
 * Manages a contiguous pool (the .user_pool section).  Allocation is
 * first-fit with immediate coalescing of freed neighbours.  Blocks are
 * aligned to UL_ARCH_REGION_ALIGN (64 bytes) so every allocation can
 * be used directly as an MPU region without re-alignment.
 *
 * Block layout (header + payload):
 *
 *   +-----------+---------- ... ----------+
 *   | blk_hdr_t | payload (aligned data)  |
 *   +-----------+---------- ... ----------+
 *
 * blk_hdr_t.size = total block size INCLUDING the header.
 * blk_hdr_t.free = 1 when block is on the free list.
 * blk_hdr_t.next/prev form a doubly-linked intrusive list of ALL blocks
 * (free and allocated) sorted by address — enables O(1) coalescing.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <kernel/include/ul_mem_internal.h>
#include <arch_config.h>

typedef struct blk_hdr {
	size_t           size;	/* block total size (header slot + payload) */
	uint32_t         free;	/* 1 = free, 0 = allocated */
	struct blk_hdr  *next;
	struct blk_hdr  *prev;
} blk_hdr_t;

/*
 * The header always occupies exactly one alignment unit so that
 * the payload (at +HDR_SIZE) is also UL_ARCH_REGION_ALIGN-aligned
 * on both 32-bit (TriCore) and 64-bit (host) targets.
 */
#define HDR_SIZE	UL_ARCH_REGION_ALIGN
#define MIN_PAYLOAD	UL_ARCH_REGION_ALIGN
#define MIN_BLOCK	(HDR_SIZE + MIN_PAYLOAD)

static blk_hdr_t *pool_head;

static inline size_t align_up(size_t n, size_t a)
{
	return (n + a - 1u) & ~(a - 1u);
}

void ul_phys_alloc_init(uintptr_t base, uintptr_t end)
{
	size_t    total;
	uint8_t  *aligned_base;
	blk_hdr_t *h;

	if (base >= end)
		return;

	/* Align pool start so the first payload is MPU-region aligned */
	aligned_base = (uint8_t *)align_up(base, UL_ARCH_REGION_ALIGN);
	total        = (size_t)(end - (uintptr_t)aligned_base);

	if (total < MIN_BLOCK)
		return;

	h = (blk_hdr_t *)(void *)aligned_base;
	h->size = total;
	h->free = 1u;
	h->next = NULL;
	h->prev = NULL;

	pool_head = h;
}

/*
 * Allocate @size bytes.  Returns a payload pointer aligned to
 * UL_ARCH_REGION_ALIGN, or NULL if the pool is exhausted.
 */
void *ul_phys_alloc(size_t size)
{
	size_t     needed;
	blk_hdr_t *b;
	blk_hdr_t *split;

	if (size == 0u)
		return NULL;

	needed = align_up(size, UL_ARCH_REGION_ALIGN) + HDR_SIZE;

	for (b = pool_head; b != NULL; b = b->next) {
		if (!b->free || b->size < needed)
			continue;

		/* Split off a new free block if the remainder is large enough */
		if (b->size >= needed + MIN_BLOCK) {
			split = (blk_hdr_t *)((uint8_t *)b + needed);
			split->size = b->size - needed;
			split->free = 1u;
			split->next = b->next;
			split->prev = b;
			if (b->next)
				b->next->prev = split;
			b->next = split;
			b->size = needed;
		}

		b->free = 0u;
		return (uint8_t *)b + HDR_SIZE;
	}

	return NULL;
}

/*
 * Free a block previously returned by ul_phys_alloc().
 * Coalesces with adjacent free blocks.
 */
void ul_phys_free(void *ptr)
{
	blk_hdr_t *b;

	if (!ptr)
		return;

	b = (blk_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
	b->free = 1u;

	/* Coalesce with next neighbour */
	if (b->next && b->next->free) {
		b->size += b->next->size;
		if (b->next->next)
			b->next->next->prev = b;
		b->next = b->next->next;
	}

	/* Coalesce with previous neighbour */
	if (b->prev && b->prev->free) {
		b->prev->size += b->size;
		b->prev->next = b->next;
		if (b->next)
			b->next->prev = b->prev;
	}
}

/* Returns the number of free bytes available (sum of free payloads). */
size_t ul_phys_free_bytes(void)
{
	size_t     total = 0u;
	blk_hdr_t *b;

	for (b = pool_head; b != NULL; b = b->next)
		if (b->free)
			total += b->size - HDR_SIZE;

	return total;
}
