/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Memory handlers — kernel/mem/mem.c
 * Implements: kernel/syscall/syscall_router.h ul_kern_mem_* prototypes
 * Reference: docs/api_spec.md §9, docs/microkernel_book_tricore.md §7
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/syscall/syscall_router.h>
#include <kernel/include/ul_thread_internal.h>
#include <kernel/include/ul_mem_internal.h>
#include <kernel/include/ul_sched.h>
#include <ul_arch.h>

/*
 * Add a region to a thread's MPU region list.
 * Returns UL_OK or UL_ENOSPC if the list is full.
 */
static int thread_add_region(ul_thread_t *th, uintptr_t base, size_t size,
			     uint32_t perms, uint8_t type)
{
	ul_arch_region_t *r;

	if (th->region_count >= UL_ARCH_MAX_REGIONS)
		return UL_ENOSPC;

	r        = &th->regions[th->region_count];
	r->base  = base;
	r->size  = size;
	r->perms = perms;
	r->type  = type;
	th->region_count++;
	return UL_OK;
}

/*
 * Remove a region from @th that starts at @base.
 * Returns UL_OK if found and removed, UL_EINVAL if not found.
 */
static int thread_remove_region(ul_thread_t *th, uintptr_t base)
{
	uint8_t i;
	uint8_t last;

	for (i = 0u; i < th->region_count; i++) {
		if (th->regions[i].base != base)
			continue;

		last = th->region_count - 1u;
		if (i != last)
			th->regions[i] = th->regions[last];
		th->region_count--;
		return UL_OK;
	}

	return UL_EINVAL;
}

uint32_t ul_kern_mem_map(uint32_t hint, uint32_t size,
			 uint32_t perms, uint32_t flags)
{
	ul_thread_t *cur = ul_sched_current();
	void        *mem;
	uintptr_t    base;
	int          rc;

	if (!cur || size == 0u)
		return (uint32_t)(int32_t)UL_EINVAL;

	if (flags & UL_MMAP_PERIPH) {
		/*
		 * Peripheral mapping: caller provides the MMIO base address.
		 * Requires UL_CAP_MAP_PERIPH (enforced in syscall_router.c).
		 */
		if (hint == 0u)
			return (uint32_t)(int32_t)UL_EINVAL;

		base = (uintptr_t)hint;
		rc   = thread_add_region(cur, base, size, perms, UL_REGION_PERIPH);
		if (rc != UL_OK)
			return (uint32_t)(int32_t)rc;

		/* Immediately apply the new region to PRS 1 DPRs. */
		ul_arch_mpu_switch(cur->regions, cur->region_count, 1u);
		return (uint32_t)base;
	}

	if (flags & UL_MMAP_ANON) {
		mem = ul_phys_alloc(size);
		if (!mem)
			return (uint32_t)(int32_t)UL_ENOMEM;

		base = (uintptr_t)mem;
		rc   = thread_add_region(cur, base, size, perms, UL_REGION_HEAP);
		if (rc != UL_OK) {
			ul_phys_free(mem);
			return (uint32_t)(int32_t)rc;
		}

		/* Immediately apply the new region to PRS 1 DPRs. */
		ul_arch_mpu_switch(cur->regions, cur->region_count, 1u);
		return (uint32_t)base;
	}

	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_mem_unmap(uint32_t addr, uint32_t size)
{
	ul_thread_t *cur = ul_sched_current();
	ul_arch_region_t *r;
	uint8_t i;

	if (!cur || addr == 0u)
		return (uint32_t)(int32_t)UL_EINVAL;

	for (i = 0u; i < cur->region_count; i++) {
		r = &cur->regions[i];
		if ((uint32_t)r->base != addr)
			continue;

		if (r->type == UL_REGION_HEAP)
			ul_phys_free((void *)(uintptr_t)addr);

		thread_remove_region(cur, (uintptr_t)addr);
		(void)size;
		return (uint32_t)UL_OK;
	}

	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_mem_grant(uint32_t addr, uint32_t size,
			   uint32_t target_tid, uint32_t perms)
{
	ul_thread_t *target;
	ul_thread_t *cur = ul_sched_current();
	ul_arch_region_t *r;
	uint8_t i;
	int     rc;

	if (!cur || addr == 0u || size == 0u)
		return (uint32_t)(int32_t)UL_EINVAL;

	/* Verify caller owns the region being granted */
	for (i = 0u; i < cur->region_count; i++) {
		if ((uint32_t)cur->regions[i].base == addr)
			break;
	}
	if (i >= cur->region_count)
		return (uint32_t)(int32_t)UL_EPERM;

	r      = &cur->regions[i];
	target = ul_thread_by_tid((ul_tid_t)target_tid);
	if (!target)
		return (uint32_t)(int32_t)UL_ESRCH;

	/* Grant read-only by default; caller may not grant more perms than held */
	uint32_t granted_perms = perms & r->perms;

	rc = thread_add_region(target, (uintptr_t)addr, r->size,
			       granted_perms, UL_REGION_SHARED);
	return (rc == UL_OK) ? (uint32_t)UL_OK : (uint32_t)(int32_t)rc;
}
