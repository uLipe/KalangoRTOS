/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Public API — ulmk
 * Full specification: docs/api_spec.md
 *
 * Usage: #include <ulmk/microkernel.h>
 *
 * All public API functions are static inline syscall wrappers.
 * They issue a TriCore SYSCALL instruction; the kernel router
 * (kernel/syscall/syscall_router.c) dispatches to the handler.
 */

#ifndef ULMK_MICROKERNEL_H
#define ULMK_MICROKERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ulmk/syscall_nr.h>

/* =========================================================================
 * Privilege levels (TriCore PSW.IO field)
 * ========================================================================= */

typedef enum {
	ULMK_PRIV_USER   = 0,	/* IO=0: no peripheral access */
	ULMK_PRIV_DRIVER = 1,	/* IO=1: peripheral access, restricted syscalls */
	ULMK_PRIV_KERNEL = 2,	/* IO=2: supervisor mode (kernel-internal use only) */
} ulmk_privilege_t;

/* =========================================================================
 * Error codes
 * ========================================================================= */

#define ULMK_OK		  0
#define ULMK_EINVAL	 -1
#define ULMK_ENOMEM	 -2
#define ULMK_EPERM	 -3
#define ULMK_ENOSPC	 -4
#define ULMK_EDEADLK	 -5
#define ULMK_ESRCH	 -6
#define ULMK_ETIMEOUT	 -7

/* =========================================================================
 * Opaque handles
 * ========================================================================= */

typedef int32_t   ulmk_tid_t;
typedef uintptr_t ulmk_ep_t;
typedef uintptr_t ulmk_notif_t;

#define ULMK_TID_INVALID		((ulmk_tid_t)-1)
#define ULMK_EP_INVALID		((ulmk_ep_t)0)
#define ULMK_NOTIF_INVALID	((ulmk_notif_t)0)

/* =========================================================================
 * IPC message
 * ========================================================================= */

#define ULMK_MSG_WORDS	6

typedef struct {
	uint32_t label;
	uint32_t words[ULMK_MSG_WORDS];
} ulmk_msg_t;

/* =========================================================================
 * Thread attributes
 * ========================================================================= */

typedef struct {
	const char	*name;
	void		(*entry)(void *arg);
	void		*arg;
	uint8_t		 priority;	/* 0 = highest, 255 = lowest */
	size_t		 stack_size;
	ulmk_privilege_t	 privilege;
	size_t		 heap_size;	/* 0 = no per-thread heap; last for compat */
} ulmk_thread_attr_t;

/*
 * Per-thread heap descriptor — returned by ulmk_get_thread_heap().
 * Describes the heap area within the thread's slabAO allocation.
 */
typedef struct {
	uintptr_t base;	/* start of the heap region */
	size_t    size;	/* size of the heap region in bytes */
} ulmk_heap_info_t;

/* =========================================================================
 * Boot information — passed to ulmk_root_thread()
 * ========================================================================= */

#define ULMK_BOOT_MAX_MEM_REGIONS	4

typedef struct {
	struct {
		uintptr_t base;
		size_t    size;
	} mem[ULMK_BOOT_MAX_MEM_REGIONS];
	uint32_t  mem_count;
	uint32_t  tick_hz;
	uintptr_t csa_pool_base;
	size_t    csa_pool_size;
} ulmk_boot_info_t;

/* =========================================================================
 * Memory domain descriptor — placed in .domain_table by ULMK_DEFINE_DOMAIN
 * ========================================================================= */

#define ULMK_PERM_READ	(1u << 0)
#define ULMK_PERM_WRITE	(1u << 1)
#define ULMK_PERM_EXEC	(1u << 2)
#define ULMK_PERM_USER	(1u << 3)

typedef struct {
	const char *name;
	uintptr_t   start;
	uintptr_t   end;
	uint32_t    perms;
} ulmk_domain_desc_t;

/* =========================================================================
 * Capability flags — bitmask stored in ulmk_thread_t.cap_flags
 *
 * Checked by the syscall router before privileged operations.
 * Root thread starts with ULMK_CAP_ALL; grants subsets to children.
 * ========================================================================= */

#define ULMK_CAP_SPAWN		(1u << 0)  /* may create threads */
#define ULMK_CAP_KILL		(1u << 1)  /* may kill other threads */
#define ULMK_CAP_IRQ		(1u << 2)  /* may bind/enable hardware IRQs */
#define ULMK_CAP_MAP_PERIPH	(1u << 3)  /* may map peripheral MMIO regions */
#define ULMK_CAP_GRANT_CAP	(1u << 4)  /* may grant capabilities to others */
#define ULMK_CAP_TIMER		(1u << 5)  /* may program the hardware timer deadline */
#define ULMK_CAP_ALL		0xFFu	   /* all capabilities; root thread initial */

/* =========================================================================
 * Memory map flags
 * ========================================================================= */

#define ULMK_MMAP_ANON	(1u << 0)
#define ULMK_MMAP_PERIPH	(1u << 1)

/* =========================================================================
 * IRQ handler type (for future ulmk_irq_* extensions)
 * ========================================================================= */

typedef void (*ulmk_irq_handler_t)(void *arg);

/* =========================================================================
 * Include ABI macros and result types after type definitions above.
 * ulmk_syscall_abi.h references ulmk_msg_t, ulmk_tid_t, so it must come last.
 * ========================================================================= */

#include <ulmk/syscall_abi.h>

/* =========================================================================
 * Boot entry point — user must provide exactly one strong definition
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info);

/* =========================================================================
 * Thread API — docs/api_spec.md §6
 * All require ULMK_PRIV_DRIVER except ulmk_thread_self() and ulmk_thread_yield().
 * ========================================================================= */

static inline ulmk_tid_t ulmk_thread_self(void)
{
	uint32_t r;
	ULMK_SYSCALL_0(ULMK_SYS_THREAD_SELF, r);
	return (ulmk_tid_t)r;
}

static inline int ulmk_thread_yield(void)
{
	uint32_t r;
	ULMK_SYSCALL_0(ULMK_SYS_YIELD, r);
	return (int)r;
}

static inline ulmk_tid_t ulmk_thread_create(const ulmk_thread_attr_t *attr)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_THREAD_SPAWN, attr, r);
	return (ulmk_tid_t)r;
}

static inline int ulmk_thread_kill(ulmk_tid_t tid)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_THREAD_KILL, tid, r);
	return (int)r;
}

static inline int ulmk_thread_suspend(ulmk_tid_t tid)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_THREAD_SUSPEND, tid, r);
	return (int)r;
}

static inline int ulmk_thread_resume(ulmk_tid_t tid)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_THREAD_RESUME, tid, r);
	return (int)r;
}

static inline int ulmk_thread_priority_set(ulmk_tid_t tid, uint8_t prio)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_THREAD_SET_PRIO, tid, prio, r);
	return (int)r;
}

static inline int ulmk_thread_priority_get(ulmk_tid_t tid)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_THREAD_GET_PRIO, tid, r);
	return (int)r;
}

static inline __attribute__((noreturn)) void ulmk_thread_exit(void)
{
	uint32_t r;
	ULMK_SYSCALL_0(ULMK_SYS_EXIT, r);
	(void)r;
	__builtin_unreachable();
}

/* =========================================================================
 * IPC Endpoint API — docs/api_spec.md §7
 * ========================================================================= */

static inline ulmk_ep_t ulmk_ep_create(void)
{
	uint32_t r;
	ULMK_SYSCALL_0(ULMK_SYS_EP_CREATE, r);
	return (ulmk_ep_t)r;
}

static inline int ulmk_ep_call(ulmk_ep_t ep, ulmk_msg_t *msg)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_EP_CALL, ep, msg, r);
	return (int)r;
}

static inline int ulmk_ep_recv(ulmk_ep_t ep, ulmk_msg_t *msg, ulmk_tid_t *sender)
{
	uint32_t r;
	ULMK_SYSCALL_3(ULMK_SYS_EP_RECV, ep, msg, sender, r);
	return (int)r;
}

static inline int ulmk_ep_reply(ulmk_tid_t sender, const ulmk_msg_t *reply)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_EP_REPLY, sender, reply, r);
	return (int)r;
}

/*
 * ulmk_ep_reply_recv — reply to sender then immediately block for the next call.
 * The three output pointers are passed in a stack-allocated args struct to
 * stay within the 4-register argument limit (ep, sender, args*).
 */
static inline int ulmk_ep_reply_recv(ulmk_ep_t ep, ulmk_tid_t sender,
				   const ulmk_msg_t *reply, ulmk_msg_t *next,
				   ulmk_tid_t *next_sender)
{
	ulmk_reply_recv_args_t args = { reply, next, next_sender };
	uint32_t r;
	ULMK_SYSCALL_3(ULMK_SYS_EP_REPLY_RECV, ep, sender, &args, r);
	return (int)r;
}

static inline int ulmk_ep_grant(ulmk_ep_t ep, ulmk_tid_t target)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_EP_GRANT, ep, target, r);
	return (int)r;
}

/*
 * ulmk_ep_recv_or_notif — block on IPC or notification, whichever arrives first.
 * Result is written into a stack-allocated ulmk_recv_or_notif_result_t and the
 * individual output pointers are filled from it after the syscall returns.
 */
static inline int ulmk_ep_recv_or_notif(ulmk_ep_t ep, ulmk_notif_t notif,
				      uint32_t mask, ulmk_msg_t *msg,
				      ulmk_tid_t *sender, uint32_t *notif_bits)
{
	ulmk_recv_or_notif_result_t res = { {0, {0}}, ULMK_TID_INVALID, 0, 0 };
	uint32_t r;
	ULMK_SYSCALL_4(ULMK_SYS_EP_RECV_OR_NOTIF, ep, notif, mask, &res, r);
	if (msg)        *msg        = res.msg;
	if (sender)     *sender     = res.sender;
	if (notif_bits) *notif_bits = res.notif_bits;
	return (int)r;
}

static inline int ulmk_ep_destroy(ulmk_ep_t ep)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_EP_DESTROY, ep, r);
	return (int)r;
}

/* =========================================================================
 * Notification API — docs/api_spec.md §8
 * ========================================================================= */

static inline ulmk_notif_t ulmk_notif_create(void)
{
	uint32_t r;
	ULMK_SYSCALL_0(ULMK_SYS_NOTIF_CREATE, r);
	return (ulmk_notif_t)r;
}

static inline int ulmk_notif_signal(ulmk_notif_t notif, uint32_t bits)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_NOTIF_SIGNAL, notif, bits, r);
	return (int)r;
}

static inline uint32_t ulmk_notif_poll(ulmk_notif_t notif, uint32_t mask)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_NOTIF_POLL, notif, mask, r);
	return r;
}

static inline int ulmk_notif_wait(ulmk_notif_t notif, uint32_t mask, uint32_t *bits)
{
	uint32_t r;
	ULMK_SYSCALL_3(ULMK_SYS_NOTIF_WAIT, notif, mask, bits, r);
	return (int)r;
}

static inline int ulmk_notif_destroy(ulmk_notif_t notif)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_NOTIF_DESTROY, notif, r);
	return (int)r;
}

/* =========================================================================
 * Memory API — docs/api_spec.md §9
 *
 * Per-thread heap model (slabAO):
 *   Each thread may carry a private heap allocated at creation time by
 *   setting attr.heap_size > 0.  The kernel allocates a contiguous slabAO
 *   (stack_size + heap_size bytes) from user_pool and covers it with a
 *   single MPU DPR.  The TCB lives in a separate allocation.
 *
 *   ulmk_get_thread_heap() — query heap base and size for the calling thread.
 *   ulmk_heap_extend()     — grow heap by allocating an additional slab from
 *                            the kernel pool; requires ULMK_PRIV_DRIVER.
 * ========================================================================= */

static inline int ulmk_get_thread_heap(ulmk_heap_info_t *info)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_GET_THREAD_HEAP, info, r);
	return (int)r;
}

static inline int ulmk_heap_extend(size_t size)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_HEAP_EXTEND, size, r);
	return (int)r;
}

static inline void *ulmk_mem_map(void *hint, size_t size,
			       uint32_t perms, uint32_t flags)
{
	uint32_t r;
	ULMK_SYSCALL_4(ULMK_SYS_MMAP, hint, size, perms, flags, r);
	return (void *)(uintptr_t)r;
}

static inline int ulmk_mem_unmap(void *addr, size_t size)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_MUNMAP, addr, size, r);
	return (int)r;
}

static inline int ulmk_mem_grant(void *addr, size_t size,
			       ulmk_tid_t target, uint32_t perms)
{
	uint32_t r;
	ULMK_SYSCALL_4(ULMK_SYS_MEM_GRANT, addr, size, target, perms, r);
	return (int)r;
}

/* =========================================================================
 * IRQ API — docs/api_spec.md §10  (requires ULMK_PRIV_DRIVER)
 * ========================================================================= */

static inline int ulmk_irq_bind(uint8_t srpn, ulmk_notif_t notif, uint32_t bit)
{
	uint32_t r;
	ULMK_SYSCALL_3(ULMK_SYS_IRQ_BIND, srpn, notif, bit, r);
	return (int)r;
}

static inline int ulmk_irq_enable(uint8_t srpn)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_IRQ_ENABLE, srpn, r);
	return (int)r;
}

static inline int ulmk_irq_disable(uint8_t srpn)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_IRQ_DISABLE, srpn, r);
	return (int)r;
}

static inline int ulmk_irq_ack(uint8_t srpn)
{
	uint32_t r;
	ULMK_SYSCALL_1(ULMK_SYS_IRQ_ACK, srpn, r);
	return (int)r;
}

/* =========================================================================
 * Capability API — docs/api_spec.md §13
 * Requires ULMK_CAP_GRANT_CAP.
 * ========================================================================= */

static inline int ulmk_cap_grant(ulmk_tid_t target, uint32_t caps)
{
	uint32_t r;
	ULMK_SYSCALL_2(ULMK_SYS_PROC_GRANT_CAP, target, caps, r);
	return (int)r;
}

/* =========================================================================
 * Timer primitives — docs/api_spec.md §11
 *
 * These are low-level kernel timer operations gated by ULMK_CAP_TIMER.
 * Only the timer server needs them.  Userspace sleep is implemented by
 * sending an IPC request to the timer server (in libul).
 *
 * ulmk_timer_set_deadline — program the hardware timer to expire at
 *   @deadline_us microseconds from now.  Only one deadline can be active
 *   at a time; a new call overwrites the previous one.
 *
 * ulmk_timer_wait — block the calling thread until the programmed deadline
 *   expires.  Exactly one thread may call this at a time.
 *
 * The 64-bit deadline value is split across two 32-bit registers
 * (D4 = low word, D5 = high word) following the TriCore ABI.
 *
 * These wrappers are excluded from kernel builds to avoid conflicts with
 * the kernel-internal ulmk_timer_set_deadline() implementation.
 * ========================================================================= */

static inline int ulmk_timer_set_deadline(uint64_t deadline_us)
{
	uint32_t lo = (uint32_t)deadline_us;
	uint32_t hi = (uint32_t)(deadline_us >> 32);
	uint32_t r;

	ULMK_SYSCALL_2(ULMK_SYS_TIMER_SETDEADLINE, lo, hi, r);
	return (int)r;
}

static inline int ulmk_timer_wait(void)
{
	uint32_t r;

	ULMK_SYSCALL_0(ULMK_SYS_TIMER_WAIT, r);
	return (int)r;
}

#endif /* ULMK_MICROKERNEL_H */
