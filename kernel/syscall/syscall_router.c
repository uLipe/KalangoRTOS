/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Syscall router — kernel/syscall/syscall_router.c
 * Reference: docs/api_spec.md §12
 *
 * Called from ulmk_arch_syscall_entry() (arch layer) which extracts the
 * syscall number and up to four arguments from arch registers before
 * reaching this function.
 *
 * Privilege enforcement: syscalls marked "IO >= 1" are gated here by
 * checking the current thread's privilege.  ULMK_PRIV_DRIVER = 1.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk/syscall_nr.h>
#include <kernel/syscall/syscall_router.h>
#include <kernel/include/ulmk_sched.h>
#include <kernel/include/ulmk_thread_internal.h>

/* Shorthand: fetch caller's privilege from the scheduler. */
static inline ulmk_privilege_t _caller_priv(void)
{
	ulmk_thread_t *t = ulmk_sched_current();
	return t ? t->privilege : ULMK_PRIV_USER;
}

#define REQUIRE_DRIVER(ret) \
	do { \
		if (_caller_priv() < ULMK_PRIV_DRIVER) \
			return (uint32_t)(int32_t)ULMK_EPERM; \
	} while (0)

/* Capability check — caller must hold @cap in cap_flags */
#define REQUIRE_CAP(cap) \
	do { \
		ulmk_thread_t *_t = ulmk_sched_current(); \
		if (!_t || !(_t->cap_flags & (cap))) \
			return (uint32_t)(int32_t)ULMK_EPERM; \
	} while (0)

uint32_t ulmk_syscall_router(uint32_t nr,
			   uint32_t a0, uint32_t a1,
			   uint32_t a2, uint32_t a3)
{
	if (nr == 0 || nr >= ULMK_SYS_MAX)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	switch (nr) {

	/* ── Memory (any privilege) ──────────────────────────────────── */
	case ULMK_SYS_MMAP:
		if (a3 & ULMK_MMAP_PERIPH)
			REQUIRE_CAP(ULMK_CAP_MAP_PERIPH);
		return ulmk_kern_mem_map(a0, a1, a2, a3);

	case ULMK_SYS_MUNMAP:
		return ulmk_kern_mem_unmap(a0, a1);

	case ULMK_SYS_MEM_GRANT:
		return ulmk_kern_mem_grant(a0, a1, a2, a3);

	case ULMK_SYS_MALLOC:
		return ulmk_kern_heap_alloc(a0);

	case ULMK_SYS_FREE:
		return ulmk_kern_heap_free(a0);

	case ULMK_SYS_ALIGNED_ALLOC:
		return ulmk_kern_heap_aligned_alloc(a0, a1);

	/* ── Scheduling (any privilege) ──────────────────────────────── */
	case ULMK_SYS_YIELD:
		return ulmk_kern_yield();

	case ULMK_SYS_EXIT:
		return ulmk_kern_exit();

	/* ── Timer primitives (requires ULMK_CAP_TIMER) ────────────────── */
	case ULMK_SYS_TIMER_SETDEADLINE:
		REQUIRE_CAP(ULMK_CAP_TIMER);
		return ulmk_kern_timer_set_deadline(a0, a1);

	case ULMK_SYS_TIMER_WAIT:
		REQUIRE_CAP(ULMK_CAP_TIMER);
		return ulmk_kern_timer_wait();

	/* ── Thread query (any privilege) ────────────────────────────── */
	case ULMK_SYS_THREAD_SELF:
		return ulmk_kern_thread_self();

	/* ── IPC endpoints (any privilege) ──────────────────────────── */
	case ULMK_SYS_EP_CREATE:
		return ulmk_kern_ep_create();

	case ULMK_SYS_EP_CALL:
		return ulmk_kern_ep_call(a0, a1);

	case ULMK_SYS_EP_RECV:
		return ulmk_kern_ep_recv(a0, a1, a2);

	case ULMK_SYS_EP_REPLY:
		return ulmk_kern_ep_reply(a0, a1);

	case ULMK_SYS_EP_REPLY_RECV:
		return ulmk_kern_ep_reply_recv(a0, a1, a2);

	case ULMK_SYS_EP_GRANT:
		return ulmk_kern_ep_grant(a0, a1);

	case ULMK_SYS_EP_RECV_OR_NOTIF:
		return ulmk_kern_ep_recv_or_notif(a0, a1, a2, a3);

	case ULMK_SYS_EP_DESTROY:
		return ulmk_kern_ep_destroy(a0);

	/* ── Notifications (any privilege) ──────────────────────────── */
	case ULMK_SYS_NOTIF_CREATE:
		return ulmk_kern_notif_create();

	case ULMK_SYS_NOTIF_SIGNAL:
		return ulmk_kern_notif_signal(a0, a1);

	case ULMK_SYS_NOTIF_WAIT:
		return ulmk_kern_notif_wait(a0, a1, a2);

	case ULMK_SYS_NOTIF_POLL:
		return ulmk_kern_notif_poll(a0, a1);

	case ULMK_SYS_NOTIF_DESTROY:
		return ulmk_kern_notif_destroy(a0);

	/* ── IRQ (requires ULMK_PRIV_DRIVER) ──────────────────────────── */
	case ULMK_SYS_IRQ_BIND:
		REQUIRE_DRIVER(a0);
		REQUIRE_CAP(ULMK_CAP_IRQ);
		return ulmk_kern_irq_bind(a0, a1, a2);

	case ULMK_SYS_IRQ_ENABLE:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_irq_enable(a0);

	case ULMK_SYS_IRQ_DISABLE:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_irq_disable(a0);

	case ULMK_SYS_IRQ_ACK:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_irq_ack(a0);

	/* ── Thread management (requires ULMK_PRIV_DRIVER) ─────────────── */
	case ULMK_SYS_THREAD_SPAWN:
		REQUIRE_DRIVER(a0);
		REQUIRE_CAP(ULMK_CAP_SPAWN);
		return ulmk_kern_thread_spawn(a0);

	case ULMK_SYS_THREAD_KILL:
		REQUIRE_DRIVER(a0);
		REQUIRE_CAP(ULMK_CAP_KILL);
		return ulmk_kern_thread_kill(a0);

	case ULMK_SYS_THREAD_SUSPEND:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_thread_suspend(a0);

	case ULMK_SYS_THREAD_RESUME:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_thread_resume(a0);

	case ULMK_SYS_THREAD_SET_PRIO:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_thread_set_prio(a0, a1);

	case ULMK_SYS_THREAD_GET_PRIO:
		REQUIRE_DRIVER(a0);
		return ulmk_kern_thread_get_prio(a0);

	/* ── Capability management (requires ULMK_CAP_GRANT_CAP) ───────── */
	case ULMK_SYS_PROC_GRANT_CAP:
		REQUIRE_DRIVER(a0);
		REQUIRE_CAP(ULMK_CAP_GRANT_CAP);
		return ulmk_kern_cap_grant(a0, a1);

	default:
		return (uint32_t)(int32_t)ULMK_EINVAL;
	}
}
