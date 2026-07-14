/* SPDX-License-Identifier: MIT */
/*
 * Syscall WCET slot — kernel/syscall/syscall_wcet.c
 *
 * Compiled always; the object is only updated when ULMK_CONFIG_SYSCALL_WCET=1
 * (see ulmk_kern_trap_syscall in kernel_main.c).
 */

#include <ulmk/syscall_wcet.h>

volatile struct ulmk_syscall_wcet_slot g_ulmk_syscall_wcet
	__attribute__((section(".user_bss")));
