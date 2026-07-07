/* SPDX-License-Identifier: MIT */
/*
 * Common userspace thread entry.
 *
 * Every thread starts here, running in user text at the thread's own
 * privilege, reached from the arch context trampoline which forwards the
 * user entry function and its argument.  If the entry function returns,
 * ulmk_thread_exit() is issued so the thread is reaped cleanly instead of
 * running off the end of its fabricated context.
 *
 * Linked into the image outside libulmk_kernel.a so it lands in the
 * userspace text region (.user_runtime), executable at every privilege
 * that a fabricated context can start in.
 */

#include <ulmk/microkernel.h>

void ulmk_user_thread_entry(void (*entry)(void *arg), void *arg);

void ulmk_user_thread_entry(void (*entry)(void *arg), void *arg)
{
	if (entry)
		entry(arg);

	ulmk_thread_exit();
}
