/* SPDX-License-Identifier: MIT */
/*
 * Kernel-side WCET helpers — pause the per-syscall cycle budget across
 * voluntary context switches so blocked RTT is not attributed to the caller.
 */

#ifndef UL_SYSCALL_WCET_INTERNAL_H
#define UL_SYSCALL_WCET_INTERNAL_H

#include <ulmk/config.h>

struct ulmk_thread;

#if ULMK_CONFIG_SYSCALL_WCET
void ulmk_syscall_wcet_account_reset(void);
uint32_t ulmk_syscall_wcet_blocked_cycles(void);
void ulmk_syscall_wcet_block_begin_th(struct ulmk_thread *th);
void ulmk_syscall_wcet_block_end_th(struct ulmk_thread *th);
#else
static inline void ulmk_syscall_wcet_account_reset(void) {}
static inline uint32_t ulmk_syscall_wcet_blocked_cycles(void) { return 0u; }
static inline void ulmk_syscall_wcet_block_begin_th(struct ulmk_thread *th)
{
	(void)th;
}
static inline void ulmk_syscall_wcet_block_end_th(struct ulmk_thread *th)
{
	(void)th;
}
#endif

#endif /* UL_SYSCALL_WCET_INTERNAL_H */
