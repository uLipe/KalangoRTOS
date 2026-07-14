/* SPDX-License-Identifier: MIT */
/*
 * Syscall WCET slot — filled by the kernel when ULMK_CONFIG_SYSCALL_WCET=1.
 *
 * Lives in .user_bss so driver threads may read it without mem_map.
 * After each syscall the kernel updates nr/delta/begin/end and increments seq.
 */

#ifndef ULMK_SYSCALL_WCET_H
#define ULMK_SYSCALL_WCET_H

#include <stdint.h>

#define ULMK_SYSCALL_WCET_MAGIC	0x57434554u	/* 'WCET' */

struct ulmk_syscall_wcet_slot {
	uint32_t magic;
	uint32_t seq;
	uint32_t nr;
	uint32_t delta;
	uint32_t begin;
	uint32_t end;
};

extern volatile struct ulmk_syscall_wcet_slot g_ulmk_syscall_wcet;

#endif /* ULMK_SYSCALL_WCET_H */
