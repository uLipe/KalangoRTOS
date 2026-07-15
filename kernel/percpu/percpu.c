/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Per-CPU state — kernel/percpu/percpu.c
 */

#include <stddef.h>
#include <string.h>
#include <kernel/include/ulmk_percpu.h>

struct ulmk_percpu g_ulmk_percpu[ULMK_NR_CPUS];

void ulmk_percpu_init(void)
{
	uint32_t i;

	for (i = 0u; i < (uint32_t)ULMK_NR_CPUS; i++) {
		memset(&g_ulmk_percpu[i], 0, sizeof(g_ulmk_percpu[i]));
		g_ulmk_percpu[i].online = (i == 0u);
	}
}
