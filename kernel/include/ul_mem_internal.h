/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Kernel memory allocator — kernel/include/ul_mem_internal.h
 * Not part of the public API.
 */

#ifndef UL_MEM_INTERNAL_H
#define UL_MEM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

void   ul_phys_alloc_init(uintptr_t base, uintptr_t end);
void  *ul_phys_alloc(size_t size);
void   ul_phys_free(void *ptr);
size_t ul_phys_free_bytes(void);

#endif /* UL_MEM_INTERNAL_H */
