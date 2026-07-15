/* SPDX-License-Identifier: MIT */
/*
 * Syscall WCET slot — filled by the kernel when ULMK_CONFIG_SYSCALL_WCET=1.
 *
 * g_ulmk_syscall_wcet[cpu] is the last sample on that CPU (debug / legacy).
 * For correct measurement when more than one thread runs on a CPU (IPC),
 * bind a private slot with ulmk_wcet_bind() — the kernel then also writes
 * that thread's samples there, without peer overwrite.
 */

#ifndef ULMK_SYSCALL_WCET_H
#define ULMK_SYSCALL_WCET_H

#include <stdint.h>

#define ULMK_SYSCALL_WCET_MAGIC		0x57434554u	/* 'WCET' */

#define ULMK_SYSCALL_WCET_MAX_CPUS	8u

struct ulmk_syscall_wcet_slot {
	uint32_t magic;
	uint32_t seq;
	uint32_t nr;
	/*
	 * delta   = (end - begin) - blocked  — pure router/handler cycles
	 * blocked = legacy mid-handler switch pause (0 with deferred
	 *           trap-exit switch; ctx switch is outside begin..end)
	 * Wall of the timed section is end - begin (== delta when blocked=0).
	 */
	uint32_t delta;
	uint32_t begin;
	uint32_t end;
	uint32_t blocked;
};

extern volatile struct ulmk_syscall_wcet_slot
	g_ulmk_syscall_wcet[ULMK_SYSCALL_WCET_MAX_CPUS];

#endif /* ULMK_SYSCALL_WCET_H */
