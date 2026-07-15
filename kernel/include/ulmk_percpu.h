/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Per-CPU kernel state — kernel/include/ulmk_percpu.h
 *
 * With ULMK_CONFIG_ENABLE_SMP=0, ULMK_NR_CPUS is forced to 1 regardless of
 * ULMK_ARCH_NUM_CPU so UP layouts match the pre-SMP kernel.
 */

#ifndef UL_PERCPU_H
#define UL_PERCPU_H

#include <stdint.h>
#include <stdbool.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>

#if ULMK_CONFIG_ENABLE_SMP
#if ULMK_ARCH_NUM_CPU <= 1
#error "ULMK_CONFIG_ENABLE_SMP requires ULMK_ARCH_NUM_CPU > 1 (board_config.h)"
#endif
#define ULMK_NR_CPUS	ULMK_ARCH_NUM_CPU
#else
#define ULMK_NR_CPUS	1
#endif

struct ulmk_thread;

struct ulmk_percpu {
	struct ulmk_thread *current;
	struct ulmk_thread *idle;
	struct ulmk_thread *dead_for_cleanup;
	ulmk_arch_ctx_t     startup_ctx;
	bool                needs_resched;
	bool                online;
#if ULMK_CONFIG_ENABLE_SMP
	/* Bitmask of remote CPUs that need a resched IPI (see sched). */
	uint32_t            ipi_pending;
#endif
};

extern struct ulmk_percpu g_ulmk_percpu[ULMK_NR_CPUS];

static inline struct ulmk_percpu *ulmk_percpu(void)
{
	uint32_t id = ulmk_arch_cpu_id();

	if (id >= (uint32_t)ULMK_NR_CPUS)
		id = 0u;
	return &g_ulmk_percpu[id];
}

static inline struct ulmk_percpu *ulmk_percpu_of(uint32_t cpu)
{
	if (cpu >= (uint32_t)ULMK_NR_CPUS)
		cpu = 0u;
	return &g_ulmk_percpu[cpu];
}

void ulmk_percpu_init(void);

#endif /* UL_PERCPU_H */
