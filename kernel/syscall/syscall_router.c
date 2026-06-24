/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Syscall router — kernel/syscall/syscall_router.c
 * Reference: docs/api_spec.md §12, docs/microkernel_book_tricore.md §15
 *
 * Called from ul_kernel_syscall_trap() (kernel_main.c) which reads the
 * TriCore registers D15 (nr) and D4–D7 (a0..a3) before the CALL that
 * reaches this function.
 *
 * Privilege enforcement: syscalls marked "IO >= 1" are gated here by
 * checking the current thread's privilege.  UL_PRIV_DRIVER = 1.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/syscall_nr.h>
#include <kernel/syscall/syscall_router.h>
#include <kernel/include/ul_sched.h>
#include <kernel/include/ul_thread_internal.h>

/* Shorthand: fetch caller's privilege from the scheduler. */
static inline ul_privilege_t _caller_priv(void)
{
	ul_thread_t *t = ul_sched_current();
	return t ? t->privilege : UL_PRIV_USER;
}

#define REQUIRE_DRIVER(ret) \
	do { \
		if (_caller_priv() < UL_PRIV_DRIVER) \
			return (uint32_t)(int32_t)UL_EPERM; \
	} while (0)

uint32_t ul_syscall_router(uint32_t nr,
			   uint32_t a0, uint32_t a1,
			   uint32_t a2, uint32_t a3)
{
	if (nr == 0 || nr >= UL_SYS_MAX)
		return (uint32_t)(int32_t)UL_EINVAL;

	switch (nr) {

	/* ── Memory (any privilege) ──────────────────────────────────── */
	case UL_SYS_MMAP:
		return ul_kern_mem_map(a0, a1, a2, a3);

	case UL_SYS_MUNMAP:
		return ul_kern_mem_unmap(a0, a1);

	case UL_SYS_MEM_GRANT:
		return ul_kern_mem_grant(a0, a1, a2, a3);

	/* ── Scheduling (any privilege) ──────────────────────────────── */
	case UL_SYS_YIELD:
		return ul_kern_yield();

	case UL_SYS_EXIT:
		return ul_kern_exit();

	/* ── Thread query (any privilege) ────────────────────────────── */
	case UL_SYS_THREAD_SELF:
		return ul_kern_thread_self();

	/* ── IPC endpoints (any privilege) ──────────────────────────── */
	case UL_SYS_EP_CREATE:
		return ul_kern_ep_create();

	case UL_SYS_EP_CALL:
		return ul_kern_ep_call(a0, a1);

	case UL_SYS_EP_RECV:
		return ul_kern_ep_recv(a0, a1, a2);

	case UL_SYS_EP_REPLY:
		return ul_kern_ep_reply(a0, a1);

	case UL_SYS_EP_REPLY_RECV:
		return ul_kern_ep_reply_recv(a0, a1, a2);

	case UL_SYS_EP_GRANT:
		return ul_kern_ep_grant(a0, a1);

	case UL_SYS_EP_RECV_OR_NOTIF:
		return ul_kern_ep_recv_or_notif(a0, a1, a2, a3);

	/* ── Notifications (any privilege) ──────────────────────────── */
	case UL_SYS_NOTIF_CREATE:
		return ul_kern_notif_create();

	case UL_SYS_NOTIF_SIGNAL:
		return ul_kern_notif_signal(a0, a1);

	case UL_SYS_NOTIF_WAIT:
		return ul_kern_notif_wait(a0, a1, a2);

	case UL_SYS_NOTIF_POLL:
		return ul_kern_notif_poll(a0, a1);

	/* ── IRQ (requires UL_PRIV_DRIVER) ──────────────────────────── */
	case UL_SYS_IRQ_BIND:
		REQUIRE_DRIVER(a0);
		return ul_kern_irq_bind(a0, a1, a2);

	case UL_SYS_IRQ_ENABLE:
		REQUIRE_DRIVER(a0);
		return ul_kern_irq_enable(a0);

	case UL_SYS_IRQ_DISABLE:
		REQUIRE_DRIVER(a0);
		return ul_kern_irq_disable(a0);

	case UL_SYS_IRQ_ACK:
		REQUIRE_DRIVER(a0);
		return ul_kern_irq_ack(a0);

	/* ── Thread management (requires UL_PRIV_DRIVER) ─────────────── */
	case UL_SYS_THREAD_SPAWN:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_spawn(a0);

	case UL_SYS_THREAD_KILL:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_kill(a0);

	case UL_SYS_THREAD_SUSPEND:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_suspend(a0);

	case UL_SYS_THREAD_RESUME:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_resume(a0);

	case UL_SYS_THREAD_SET_PRIO:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_set_prio(a0, a1);

	case UL_SYS_THREAD_GET_PRIO:
		REQUIRE_DRIVER(a0);
		return ul_kern_thread_get_prio(a0);

	default:
		return (uint32_t)(int32_t)UL_EINVAL;
	}
}
