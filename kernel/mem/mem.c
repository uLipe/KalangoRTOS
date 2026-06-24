/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Memory handlers — kernel/mem/mem.c
 * Implements: syscall_router.h ul_kern_mem_* prototypes
 * Reference: docs/api_spec.md §9, docs/microkernel_book_tricore.md §7
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

uint32_t ul_kern_mem_map(uint32_t hint, uint32_t size,
			 uint32_t perms, uint32_t flags)
{
	(void)hint;
	(void)size;
	(void)perms;
	(void)flags;
	/*
	 * TODO:
	 *   UL_MMAP_ANON:   ul_arch_phys_alloc(size, UL_ARCH_REGION_ALIGN)
	 *                   ul_arch_mpu_configure(PRS, new_region, ...)
	 *   UL_MMAP_PERIPH: validate against peripheral allow-list
	 *                   ul_arch_mpu_configure(PRS, periph_region, ...)
	 */
	return 0; /* NULL on failure */
}

uint32_t ul_kern_mem_unmap(uint32_t addr, uint32_t size)
{
	(void)addr;
	(void)size;
	/* TODO: remove MPU region, ul_arch_phys_free() */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_mem_grant(uint32_t addr, uint32_t size,
			   uint32_t target_tid, uint32_t perms)
{
	(void)addr;
	(void)size;
	(void)target_tid;
	(void)perms;
	/* TODO: add region to target thread's MPU context */
	return (uint32_t)(int32_t)UL_EINVAL;
}
