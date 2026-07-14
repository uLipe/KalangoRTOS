/* SPDX-License-Identifier: MIT */
/*
 * Syscall WCET slot — filled by the kernel when ULMK_CONFIG_SYSCALL_WCET=1.
 *
 * Lives in .user_bss so driver threads may read it without mem_map.
 * After each syscall the kernel updates nr/delta/begin/end/blocked and
 * increments seq.
 */

#ifndef ULMK_SYSCALL_WCET_H
#define ULMK_SYSCALL_WCET_H

#include <stdint.h>

#define ULMK_SYSCALL_WCET_MAGIC	0x57434554u	/* 'WCET' */

struct ulmk_syscall_wcet_slot {
	uint32_t magic;
	uint32_t seq;
	uint32_t nr;
	/*
	 * delta   = (end - begin) - blocked  — pure gateway/handler cycles
	 * blocked = cycles spent away from this thread (ctx_switch RTT)
	 * Wall clock of the timed section is begin..end.
	 */
	uint32_t delta;
	uint32_t begin;
	uint32_t end;
	uint32_t blocked;
};

extern volatile struct ulmk_syscall_wcet_slot g_ulmk_syscall_wcet;

#endif /* ULMK_SYSCALL_WCET_H */
