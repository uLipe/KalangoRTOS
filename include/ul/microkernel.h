/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Public API — ulipeMicroKernel
 * Full specification: docs/api_spec.md
 *
 * Usage: #include <ul/microkernel.h>
 *
 * All public API functions are static inline syscall wrappers.
 * They issue a TriCore SYSCALL instruction; the kernel router
 * (kernel/syscall/syscall_router.c) dispatches to the handler.
 */

#ifndef UL_MICROKERNEL_H
#define UL_MICROKERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ul/syscall_nr.h>

/* =========================================================================
 * Privilege levels (TriCore PSW.IO field)
 * ========================================================================= */

typedef enum {
	UL_PRIV_USER   = 0,	/* IO=0: no peripheral access */
	UL_PRIV_DRIVER = 1,	/* IO=1: peripheral access, restricted syscalls */
	UL_PRIV_KERNEL = 2,	/* IO=2: supervisor mode (kernel-internal use only) */
} ul_privilege_t;

/* =========================================================================
 * Error codes
 * ========================================================================= */

#define UL_OK		  0
#define UL_EINVAL	 -1
#define UL_ENOMEM	 -2
#define UL_EPERM	 -3
#define UL_ENOSPC	 -4
#define UL_EDEADLK	 -5
#define UL_ESRCH	 -6
#define UL_ETIMEOUT	 -7

/* =========================================================================
 * Opaque handles
 * ========================================================================= */

typedef int32_t  ul_tid_t;
typedef int32_t  ul_ep_t;
typedef int32_t  ul_notif_t;

#define UL_TID_INVALID		((ul_tid_t)-1)
#define UL_EP_INVALID		((ul_ep_t)-1)
#define UL_NOTIF_INVALID	((ul_notif_t)-1)

/* =========================================================================
 * IPC message
 * ========================================================================= */

#define UL_MSG_WORDS	4

typedef struct {
	uint32_t label;
	uint32_t words[UL_MSG_WORDS];
} ul_msg_t;

/* =========================================================================
 * Thread attributes
 * ========================================================================= */

typedef struct {
	const char	*name;
	void		(*entry)(void *arg);
	void		*arg;
	uint8_t		 priority;	/* 0 = highest, 255 = lowest */
	size_t		 stack_size;
	ul_privilege_t	 privilege;
} ul_thread_attr_t;

/* =========================================================================
 * Boot information — passed to ul_root_thread()
 * ========================================================================= */

#define UL_BOOT_MAX_MEM_REGIONS	4

typedef struct {
	struct {
		uintptr_t base;
		size_t    size;
	} mem[UL_BOOT_MAX_MEM_REGIONS];
	uint32_t  mem_count;
	uint32_t  tick_hz;
	uintptr_t csa_pool_base;
	size_t    csa_pool_size;
} ul_boot_info_t;

/* =========================================================================
 * Memory domain descriptor — placed in .domain_table by UL_DEFINE_DOMAIN
 * ========================================================================= */

#define UL_PERM_READ	(1u << 0)
#define UL_PERM_WRITE	(1u << 1)
#define UL_PERM_EXEC	(1u << 2)
#define UL_PERM_USER	(1u << 3)

typedef struct {
	const char *name;
	uintptr_t   start;
	uintptr_t   end;
	uint32_t    perms;
} ul_domain_desc_t;

/* =========================================================================
 * Memory map flags
 * ========================================================================= */

#define UL_MMAP_ANON	(1u << 0)
#define UL_MMAP_PERIPH	(1u << 1)

/* =========================================================================
 * IRQ handler type (for future ul_irq_* extensions)
 * ========================================================================= */

typedef void (*ul_irq_handler_t)(void *arg);

/* =========================================================================
 * Include ABI macros and result types after type definitions above.
 * ul_syscall_abi.h references ul_msg_t, ul_tid_t, so it must come last.
 * ========================================================================= */

#include <ul/syscall_abi.h>

/* =========================================================================
 * Boot entry point — user must provide exactly one strong definition
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info);

/* =========================================================================
 * Thread API — docs/api_spec.md §6
 * All require UL_PRIV_DRIVER except ul_thread_self() and ul_thread_yield().
 * ========================================================================= */

static inline ul_tid_t ul_thread_self(void)
{
	uint32_t r;
	UL_SYSCALL_0(UL_SYS_THREAD_SELF, r);
	return (ul_tid_t)r;
}

static inline int ul_thread_yield(void)
{
	uint32_t r;
	UL_SYSCALL_0(UL_SYS_YIELD, r);
	return (int)r;
}

static inline ul_tid_t ul_thread_create(const ul_thread_attr_t *attr)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_THREAD_SPAWN, attr, r);
	return (ul_tid_t)r;
}

static inline int ul_thread_kill(ul_tid_t tid)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_THREAD_KILL, tid, r);
	return (int)r;
}

static inline int ul_thread_suspend(ul_tid_t tid)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_THREAD_SUSPEND, tid, r);
	return (int)r;
}

static inline int ul_thread_resume(ul_tid_t tid)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_THREAD_RESUME, tid, r);
	return (int)r;
}

static inline int ul_thread_priority_set(ul_tid_t tid, uint8_t prio)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_THREAD_SET_PRIO, tid, prio, r);
	return (int)r;
}

static inline int ul_thread_priority_get(ul_tid_t tid)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_THREAD_GET_PRIO, tid, r);
	return (int)r;
}

static inline __attribute__((noreturn)) void ul_thread_exit(void)
{
	uint32_t r;
	UL_SYSCALL_0(UL_SYS_EXIT, r);
	(void)r;
	__builtin_unreachable();
}

/* =========================================================================
 * IPC Endpoint API — docs/api_spec.md §7
 * ========================================================================= */

static inline ul_ep_t ul_ep_create(void)
{
	uint32_t r;
	UL_SYSCALL_0(UL_SYS_EP_CREATE, r);
	return (ul_ep_t)r;
}

static inline int ul_ep_call(ul_ep_t ep, ul_msg_t *msg)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_EP_CALL, ep, msg, r);
	return (int)r;
}

static inline int ul_ep_recv(ul_ep_t ep, ul_msg_t *msg, ul_tid_t *sender)
{
	uint32_t r;
	UL_SYSCALL_3(UL_SYS_EP_RECV, ep, msg, sender, r);
	return (int)r;
}

static inline int ul_ep_reply(ul_tid_t sender, const ul_msg_t *reply)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_EP_REPLY, sender, reply, r);
	return (int)r;
}

/*
 * ul_ep_reply_recv — reply to sender then immediately block for the next call.
 * The three output pointers are passed in a stack-allocated args struct to
 * stay within the 4-register argument limit (ep, sender, args*).
 */
static inline int ul_ep_reply_recv(ul_ep_t ep, ul_tid_t sender,
				   const ul_msg_t *reply, ul_msg_t *next,
				   ul_tid_t *next_sender)
{
	ul_reply_recv_args_t args = { reply, next, next_sender };
	uint32_t r;
	UL_SYSCALL_3(UL_SYS_EP_REPLY_RECV, ep, sender, &args, r);
	return (int)r;
}

static inline int ul_ep_grant(ul_ep_t ep, ul_tid_t target)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_EP_GRANT, ep, target, r);
	return (int)r;
}

/*
 * ul_ep_recv_or_notif — block on IPC or notification, whichever arrives first.
 * Result is written into a stack-allocated ul_recv_or_notif_result_t and the
 * individual output pointers are filled from it after the syscall returns.
 */
static inline int ul_ep_recv_or_notif(ul_ep_t ep, ul_notif_t notif,
				      uint32_t mask, ul_msg_t *msg,
				      ul_tid_t *sender, uint32_t *notif_bits)
{
	ul_recv_or_notif_result_t res = { {0, {0}}, UL_TID_INVALID, 0, 0 };
	uint32_t r;
	UL_SYSCALL_4(UL_SYS_EP_RECV_OR_NOTIF, ep, notif, mask, &res, r);
	if (msg)        *msg        = res.msg;
	if (sender)     *sender     = res.sender;
	if (notif_bits) *notif_bits = res.notif_bits;
	return (int)r;
}

/* =========================================================================
 * Notification API — docs/api_spec.md §8
 * ========================================================================= */

static inline ul_notif_t ul_notif_create(void)
{
	uint32_t r;
	UL_SYSCALL_0(UL_SYS_NOTIF_CREATE, r);
	return (ul_notif_t)r;
}

static inline int ul_notif_signal(ul_notif_t notif, uint32_t bits)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_NOTIF_SIGNAL, notif, bits, r);
	return (int)r;
}

static inline uint32_t ul_notif_poll(ul_notif_t notif, uint32_t mask)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_NOTIF_POLL, notif, mask, r);
	return r;
}

static inline int ul_notif_wait(ul_notif_t notif, uint32_t mask, uint32_t *bits)
{
	uint32_t r;
	UL_SYSCALL_3(UL_SYS_NOTIF_WAIT, notif, mask, bits, r);
	return (int)r;
}

/* =========================================================================
 * Memory API — docs/api_spec.md §9
 * ========================================================================= */

static inline void *ul_mem_map(void *hint, size_t size,
			       uint32_t perms, uint32_t flags)
{
	uint32_t r;
	UL_SYSCALL_4(UL_SYS_MMAP, hint, size, perms, flags, r);
	return (void *)(uintptr_t)r;
}

static inline int ul_mem_unmap(void *addr, size_t size)
{
	uint32_t r;
	UL_SYSCALL_2(UL_SYS_MUNMAP, addr, size, r);
	return (int)r;
}

static inline int ul_mem_grant(void *addr, size_t size,
			       ul_tid_t target, uint32_t perms)
{
	uint32_t r;
	UL_SYSCALL_4(UL_SYS_MEM_GRANT, addr, size, target, perms, r);
	return (int)r;
}

/* =========================================================================
 * IRQ API — docs/api_spec.md §10  (requires UL_PRIV_DRIVER)
 * ========================================================================= */

static inline int ul_irq_bind(uint8_t srpn, ul_notif_t notif, uint32_t bit)
{
	uint32_t r;
	UL_SYSCALL_3(UL_SYS_IRQ_BIND, srpn, notif, bit, r);
	return (int)r;
}

static inline int ul_irq_enable(uint8_t srpn)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_IRQ_ENABLE, srpn, r);
	return (int)r;
}

static inline int ul_irq_disable(uint8_t srpn)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_IRQ_DISABLE, srpn, r);
	return (int)r;
}

static inline int ul_irq_ack(uint8_t srpn)
{
	uint32_t r;
	UL_SYSCALL_1(UL_SYS_IRQ_ACK, srpn, r);
	return (int)r;
}

/* =========================================================================
 * Sleep API — docs/api_spec.md §11
 *
 * Durations must be non-zero positive values.  The kernel treats duration 0
 * as an immediate yield (no error).  The 64-bit µs value is split across
 * two 32-bit argument registers (D4 = low word, D5 = high word).
 * ========================================================================= */

static inline int ul_usleep(uint64_t us)
{
	uint32_t lo = (uint32_t)us;
	uint32_t hi = (uint32_t)(us >> 32);
	uint32_t r;

	UL_SYSCALL_2(UL_SYS_SLEEP_US, lo, hi, r);
	return (int)r;
}

static inline int ul_msleep(uint32_t ms)
{
	return ul_usleep((uint64_t)ms * 1000ULL);
}

static inline int ul_sleep(uint32_t seconds)
{
	return ul_usleep((uint64_t)seconds * 1000000ULL);
}

#endif /* UL_MICROKERNEL_H */
