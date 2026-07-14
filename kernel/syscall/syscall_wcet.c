/* SPDX-License-Identifier: MIT */
/*
 * Syscall WCET slot — kernel/syscall/syscall_wcet.c
 *
 * Compiled always; the object is only updated when ULMK_CONFIG_SYSCALL_WCET=1
 * (see ulmk_kern_trap_syscall in kernel_main.c).
 */

#include <ulmk/syscall_wcet.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/ulmk_thread_internal.h>

volatile struct ulmk_syscall_wcet_slot g_ulmk_syscall_wcet
	__attribute__((section(".user_bss")));

#if ULMK_CONFIG_SYSCALL_WCET

void ulmk_syscall_wcet_account_reset(void)
{
	ulmk_thread_t *cur = ulmk_sched_current();

	if (!cur)
		return;
	cur->wcet_blocked    = 0u;
	cur->wcet_block_open = 0u;
}

uint32_t ulmk_syscall_wcet_blocked_cycles(void)
{
	ulmk_thread_t *cur = ulmk_sched_current();

	return cur ? cur->wcet_blocked : 0u;
}

void ulmk_syscall_wcet_block_begin_th(struct ulmk_thread *th)
{
	ulmk_thread_t *t = th;

	if (!t || t->wcet_block_open)
		return;
	t->wcet_block_mark = ulmk_arch_cycle_read();
	t->wcet_block_open = 1u;
}

void ulmk_syscall_wcet_block_end_th(struct ulmk_thread *th)
{
	ulmk_thread_t *t = th;
	uint32_t       now;

	if (!t || !t->wcet_block_open)
		return;
	now = ulmk_arch_cycle_read();
	t->wcet_blocked += now - t->wcet_block_mark;
	t->wcet_block_open = 0u;
}

#endif /* ULMK_CONFIG_SYSCALL_WCET */
