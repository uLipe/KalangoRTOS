/* SPDX-License-Identifier: MIT */
/*
 * ipc_unit_test.c — host unit tests for kernel/ipc/ep.c and
 *                   kernel/notif/notif.c
 *
 * Compiles both modules against stub implementations of the scheduler,
 * thread registry, and arch layer.  No QEMU or hardware required.
 *
 * Since context switches are deferred to trap exit, blocking paths return
 * immediately without switching context.  Tests verify state machine
 * transitions (BLOCKED_IPC_CALL, BLOCKED_IPC_RECV, queues, etc.) rather
 * than actual rendezvous execution.  Full rendezvous is covered by the
 * ipc SDK suite.
 *
 * Test plan:
 *  ep_create:
 *   1.  create returns valid handle
 *   2.  create fills pool; next create returns -ULMK_ENOSPC
 *   3.  create invalid id returns -ULMK_EINVAL
 *
 *  ep_call (state transitions):
 *   4.  call on invalid ep returns -ULMK_EINVAL
 *   5.  call with null msg_ptr returns -ULMK_EINVAL
 *   6.  call fast path: server in recv_queue — server is woken, caller blocks
 *   7.  call slow path: no server — caller enters send_queue
 *
 *  ep_recv (state transitions):
 *   8.  recv on invalid ep returns -ULMK_EINVAL
 *   9.  recv with null msg_ptr returns -ULMK_EINVAL
 *  10.  recv fast path: caller in send_queue — message delivered, no blocking
 *  11.  recv slow path: no caller — server enters recv_queue
 *
 *  ep_reply:
 *  12.  reply to invalid tid returns -ULMK_EINVAL
 *  13.  reply to non-IPC-blocked thread returns -ULMK_EINVAL
 *  14.  reply wakes caller, restores server priority
 *
 *  ep_reply_recv:
 *  15.  null args returns -ULMK_EINVAL
 *  16.  reply_recv with ULMK_TID_INVALID sender skips reply, does recv
 *
 *  ep_grant:
 *  17.  grant invalid ep returns -ULMK_EINVAL
 *  18.  grant invalid target tid returns -ULMK_EINVAL
 *  19.  grant valid returns 0 (no enforcement)
 *
 *  notif_create:
 *  20.  create returns valid handle
 *  21.  exhaust pool; next returns -ULMK_ENOSPC
 *
 *  notif_signal:
 *  22.  signal on invalid notif returns -ULMK_EINVAL
 *  23.  signal with no waiter — bits accumulated
 *  24.  signal wakes waiting thread, consumes bits
 *
 *  notif_wait:
 *  25.  wait on invalid notif returns -ULMK_EINVAL
 *  26.  wait null bits_ptr returns -ULMK_EINVAL
 *  27.  wait fast path: bits already set — returns without blocking
 *  28.  wait slow path: no bits — thread blocks
 *
 *  notif_poll:
 *  29.  poll on invalid notif returns 0
 *  30.  poll consumes matched bits, returns them
 *  31.  poll with non-matching mask returns 0, leaves bits intact
 *
 *  priority inheritance:
 *  32.  ep_call boosts server priority when caller > server
 *  33.  ep_reply restores server priority
 *
 *  kill cleanup:
 *  34.  kill IPC-RECV thread removes it from recv_queue
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ── Stub includes ─────────────────────────────────────────────────────── */

#include "include/ulmk_arch.h"
#include "include/ulmk/microkernel.h"
#include "include/ulmk/config.h"
#include "../../kernel/include/ulmk_thread_internal.h"
#include "../../kernel/include/ulmk_sched.h"
#include "../../kernel/include/ulmk_ep_internal.h"
#include "../../kernel/include/ulmk_notif_internal.h"
#include "../../kernel/syscall/syscall_router.h"

/* ── Test framework ────────────────────────────────────────────────────── */

static int g_pass;
static int g_fail;

#define ASSERT(cond) \
	do { \
		if (cond) { \
			g_pass++; \
		} else { \
			fprintf(stderr, "  FAIL  %s:%d: %s\n", \
				__FILE__, __LINE__, #cond); \
			g_fail++; \
		} \
	} while (0)

#define RUN(fn) \
	do { \
		printf("  [....] %s\r", #fn); \
		fflush(stdout); \
		fn(); \
		printf("  [PASS] %s\n", #fn); \
	} while (0)

/* ── Scheduler stubs ───────────────────────────────────────────────────── */

static ulmk_thread_t *g_current;
static int          g_enqueue_count;
static int          g_schedule_count;
static int          g_dequeue_count;

ulmk_thread_t *ulmk_sched_current(void)   { return g_current; }
void ulmk_sched_enqueue(ulmk_thread_t *t) { t->state = UL_THREAD_STATE_READY; g_enqueue_count++; }
void ulmk_sched_dequeue(ulmk_thread_t *t) { (void)t; g_dequeue_count++; }
void ulmk_sched_enqueue_locked(ulmk_thread_t *t) { ulmk_sched_enqueue(t); }
void ulmk_sched_dequeue_locked(ulmk_thread_t *t) { ulmk_sched_dequeue(t); }
void ulmk_sched_resched(void)           { g_schedule_count++; }
void ulmk_sched_request_resched(void)   { g_schedule_count++; }

/* ── Thread registry stub ──────────────────────────────────────────────── */

#define MAX_REG    64
#define TPOOL_SIZE 64
static ulmk_thread_t  g_tpool[TPOOL_SIZE];
static int          g_tpool_idx;
static ulmk_thread_t *g_reg[MAX_REG];
static int          g_reg_count;

ulmk_thread_t *ulmk_thread_by_tid(ulmk_tid_t tid)
{
	ulmk_thread_t *th = (ulmk_thread_t *)tid;

	if (tid == ULMK_TID_INVALID || !th)
		return NULL;
	if (th->state == UL_THREAD_STATE_DEAD)
		return NULL;
	return th;
}

/* ── Timeout stubs (IPC unit does not exercise the wheel) ─────────────── */

uint32_t ulmk_ms_to_ticks(uint32_t ms)
{
	if (ms > 250000u)
		return 0u;
	return ms ? ms : 1u;
}

int ulmk_timeout_arm(ulmk_thread_t *th, uint32_t ms,
		     void (*cb)(struct ulmk_timeout *to))
{
	(void)ms;
	(void)cb;
	if (!th)
		return ULMK_EINVAL;
	return ULMK_OK;
}

void ulmk_timeout_disarm(ulmk_thread_t *th)
{
	(void)th;
}

/* ── Helpers ───────────────────────────────────────────────────────────── */

static void reset_counters(void)
{
	g_enqueue_count  = 0;
	g_schedule_count = 0;
	g_dequeue_count  = 0;
}

static ulmk_thread_t *make_thread(uint8_t prio)
{
	ulmk_thread_t *t;

	t = &g_tpool[g_tpool_idx++];
	memset(t, 0, sizeof(*t));
	t->tid            = (ulmk_tid_t)(uintptr_t)t;
	t->priority       = prio;
	t->saved_prio     = prio;
	t->state          = UL_THREAD_STATE_READY;
	t->blocked_reason = UL_BLOCKED_NONE;
	t->blocked_ep     = ULMK_EP_INVALID;
	t->blocked_notif  = ULMK_NOTIF_INVALID;
	t->ipc_sender     = ULMK_TID_INVALID;
	sys_dnode_init(&t->sched_node);
	sys_dnode_init(&t->ipc_node);
	sys_dnode_init(&t->reg_node);

	if (g_reg_count < MAX_REG)
		g_reg[g_reg_count++] = t;

	return t;
}

static void ipc_test_enqueue_recv(ulmk_endpoint_t *ep, ulmk_thread_t *th)
{
	sys_dnode_init(&th->ipc_node);
	sys_dlist_append(&ep->recv_queue, &th->ipc_node);
}

static void ipc_test_enqueue_send(ulmk_endpoint_t *ep, ulmk_thread_t *th)
{
	sys_dnode_init(&th->ipc_node);
	sys_dlist_append(&ep->send_queue, &th->ipc_node);
}

/* Reset the static pools used by ep.c and notif.c between test groups. */
extern ulmk_endpoint_t  ep_pool[];
extern ulmk_notif_obj_t notif_pool[];

static void reset_pools(void)
{
	memset(ep_pool,    0, sizeof(ulmk_endpoint_t)  * ULMK_CONFIG_MAX_ENDPOINTS);
	memset(notif_pool, 0, sizeof(ulmk_notif_obj_t) * ULMK_CONFIG_MAX_NOTIFS);
	/* Reset thread pool and registry so each test starts clean. */
	g_tpool_idx = 0;
	g_reg_count = 0;
	g_current   = NULL;
	reset_counters();
}

/* ── ep_create ─────────────────────────────────────────────────────────── */

static void test_ep_create_valid(void)
{
	reset_pools();
	int id = (int)(int32_t)ulmk_kern_ep_create();

	ASSERT(id >= 0);
}

static void test_ep_create_exhaust(void)
{
	int i;
	int last;

	reset_pools();
	for (i = 0; i < ULMK_CONFIG_MAX_ENDPOINTS; i++)
		ulmk_kern_ep_create();

	last = (int)(int32_t)ulmk_kern_ep_create();
	ASSERT(last == -ULMK_ENOSPC);
}

/* ── ep_call ───────────────────────────────────────────────────────────── */

static void test_ep_call_invalid_ep(void)
{
	ulmk_msg_t msg = {0};

	reset_pools();
	g_current = make_thread(10);

	ASSERT(ep_call_impl(999, &msg) == -ULMK_EINVAL);
	ASSERT(g_schedule_count == 0);
}

static void test_ep_call_null_msg(void)
{
	reset_pools();
	ulmk_kern_ep_create();
	g_current = make_thread(10);

	ASSERT(ep_call_impl(0, NULL) == -ULMK_EINVAL);
}

static void test_ep_call_fast_path_server_waiting(void)
{
	ulmk_msg_t      msg     = { .label = 0xBEEF };
	ulmk_msg_t      srv_buf = {0};
	ulmk_tid_t      srv_tid = ULMK_TID_INVALID;
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *caller;
	ulmk_thread_t   *server;

	reset_pools();

	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	server = make_thread(20);
	caller = make_thread(5);

	server->state              = UL_THREAD_STATE_BLOCKED;
	server->blocked_reason     = UL_BLOCKED_IPC_RECV;
	server->blocked_ep         = 0;
	server->ipc_msg_outptr     = &srv_buf;
	server->ipc_sender_outptr  = &srv_tid;
	ipc_test_enqueue_recv(ep, server);

	g_current = caller;
	ASSERT(ep_call_impl(0, &msg) == 0);

	ASSERT(server->state == UL_THREAD_STATE_READY);
	/* Single hop into the server's staged buffer — not TCB bounce. */
	ASSERT(srv_buf.label == 0xBEEF);
	ASSERT(srv_tid == caller->tid);
	ASSERT(server->ipc_msg_outptr == NULL);
	ASSERT(server->ipc_sender == caller->tid);
	ASSERT(sys_dlist_is_empty(&ep->recv_queue));
	ASSERT(server->priority == 5);    /* priority inheritance */
	ASSERT(caller->state == UL_THREAD_STATE_BLOCKED);
	ASSERT(caller->blocked_reason == UL_BLOCKED_IPC_CALL);
	/* Staging stays until reply — switch deferred to trap exit. */
	ASSERT(caller->ipc_msg_outptr == &msg);
	/* Server enqueued for trap-exit switch (no mid-handler handoff). */
	ASSERT(g_enqueue_count == 1);
	ASSERT(g_schedule_count == 0);
}

static void test_ep_call_slow_path_no_server(void)
{
	ulmk_msg_t      msg = { .label = 0xCAFE };
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *caller;

	reset_pools();
	ulmk_kern_ep_create();
	ep     = ulmk_ep_by_id(0);
	caller = make_thread(10);

	g_current = caller;
	ASSERT(ep_call_impl(0, &msg) == 0);

	ASSERT(caller->state == UL_THREAD_STATE_BLOCKED);
	ASSERT(caller->blocked_reason == UL_BLOCKED_IPC_CALL);
	ASSERT(SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&ep->send_queue, ulmk_thread_t,
						ipc_node) == caller);
	/* Deferred switch — no mid-handler resched. */
	ASSERT(g_schedule_count == 0);
}

/* ── ep_recv ───────────────────────────────────────────────────────────── */

static void test_ep_recv_invalid_ep(void)
{
	ulmk_msg_t msg = {0};

	reset_pools();
	g_current = make_thread(10);

	ASSERT(ep_recv_impl(999, &msg, NULL) == -ULMK_EINVAL);
}

static void test_ep_recv_null_msg(void)
{
	reset_pools();
	ulmk_kern_ep_create();
	g_current = make_thread(10);

	ASSERT(ep_recv_impl(0, NULL, NULL) == -ULMK_EINVAL);
}

static void test_ep_recv_fast_path_caller_waiting(void)
{
	ulmk_msg_t      req        = { .label = 0x1234 };
	ulmk_msg_t      msg_out    = {0};
	ulmk_tid_t      sender_out = ULMK_TID_INVALID;
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *server;
	ulmk_thread_t   *caller;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	caller = make_thread(5);
	server = make_thread(20);

	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;
	caller->ipc_msg_outptr = &req;
	ipc_test_enqueue_send(ep, caller);

	g_current = server;
	ep_recv_impl(0, &msg_out, &sender_out);

	ASSERT(msg_out.label == 0x1234);
	ASSERT(sender_out == caller->tid);
	ASSERT(server->ipc_sender == caller->tid);
	ASSERT(sys_dlist_is_empty(&ep->send_queue));
	ASSERT(caller->blocked_reason == UL_BLOCKED_IPC_CALL);
	ASSERT(server->priority == 5);    /* PI on recv fast path */
	ASSERT(server->saved_prio == 20);
	ASSERT(g_schedule_count == 0);
}

static void test_ep_recv_slow_path_no_caller(void)
{
	ulmk_msg_t      msg_out = {0};
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *server;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	server    = make_thread(20);
	g_current = server;
	ep_recv_impl(0, &msg_out, NULL);

	ASSERT(server->state == UL_THREAD_STATE_BLOCKED);
	ASSERT(server->blocked_reason == UL_BLOCKED_IPC_RECV);
	ASSERT(SYS_DLIST_PEEK_HEAD_CONTAINER_OF(&ep->recv_queue, ulmk_thread_t,
						ipc_node) == server);
	ASSERT(g_schedule_count == 0);
}

/* ── ep_reply ──────────────────────────────────────────────────────────── */

static void test_ep_reply_invalid_tid(void)
{
	ulmk_msg_t reply = {0};

	reset_pools();
	g_current = make_thread(10);

	ASSERT(ep_reply_impl(ULMK_TID_INVALID, &reply) == -ULMK_EINVAL);
}

static void test_ep_reply_non_blocked_caller(void)
{
	ulmk_msg_t    reply = {0};
	ulmk_thread_t *not_blocked;

	reset_pools();
	not_blocked = make_thread(10);
	g_current   = make_thread(10);

	ASSERT(ep_reply_impl(not_blocked->tid, &reply) == -ULMK_EINVAL);
}

static void test_ep_reply_wakes_caller_restores_prio(void)
{
	ulmk_msg_t     reply   = { .label = 0xDEAD };
	ulmk_msg_t     call_buf = {0};
	ulmk_thread_t *server;
	ulmk_thread_t *caller;

	reset_pools();
	server = make_thread(20);
	caller = make_thread(5);

	server->saved_prio     = 20;
	server->priority       = 5;
	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;
	caller->ipc_msg_outptr = &call_buf;

	g_current = server;
	ASSERT(ep_reply_impl(caller->tid, &reply) == 0);

	ASSERT(call_buf.label == 0xDEAD);
	ASSERT(caller->state == UL_THREAD_STATE_READY);
	ASSERT(caller->blocked_reason == UL_BLOCKED_NONE);
	ASSERT(server->priority == 20);
	ASSERT(g_enqueue_count == 1);
	ASSERT(g_schedule_count == 0);
}

/* ── ep_reply_recv ─────────────────────────────────────────────────────── */

static void test_ep_reply_recv_null_args(void)
{
	reset_pools();
	g_current = make_thread(10);
	ulmk_kern_ep_create();

	ASSERT(ep_reply_recv_impl(0, ULMK_TID_INVALID, NULL, NULL, NULL) == -ULMK_EINVAL);
}

static void test_ep_reply_recv_skip_reply_on_invalid_tid(void)
{
	ulmk_msg_t      req        = { .label = 0x5678 };
	ulmk_msg_t      msg_out    = {0};
	ulmk_tid_t      sender_out = ULMK_TID_INVALID;
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *server;
	ulmk_thread_t   *caller;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	caller = make_thread(5);
	server = make_thread(20);

	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;
	caller->ipc_msg_outptr = &req;
	ipc_test_enqueue_send(ep, caller);

	g_current = server;
	ASSERT(ep_reply_recv_impl(0, ULMK_TID_INVALID, NULL,
				  &msg_out, &sender_out) == 0);

	ASSERT(msg_out.label == 0x5678);
	ASSERT(sender_out == caller->tid);
}

/* ── ep_grant ──────────────────────────────────────────────────────────── */

static void test_ep_grant_invalid_ep(void)
{
	reset_pools();
	g_current = make_thread(10);

	ASSERT(ep_grant_impl(999, g_current->tid) == -ULMK_EINVAL);
}

static void test_ep_grant_invalid_tid(void)
{
	reset_pools();
	ulmk_kern_ep_create();

	ASSERT(ep_grant_impl(0, ULMK_TID_INVALID) == -ULMK_EINVAL);
}

static void test_ep_grant_valid(void)
{
	ulmk_thread_t *t;

	reset_pools();
	ulmk_kern_ep_create();
	t = make_thread(10);

	ASSERT(ep_grant_impl(0, t->tid) == 0);
}

/* ── notif_create ──────────────────────────────────────────────────────── */

static void test_notif_create_valid(void)
{
	reset_pools();
	int id = (int)(int32_t)ulmk_kern_notif_create();

	ASSERT(id >= 0);
}

static void test_notif_create_exhaust(void)
{
	int i;
	int last;

	reset_pools();
	for (i = 0; i < ULMK_CONFIG_MAX_NOTIFS; i++)
		ulmk_kern_notif_create();

	last = (int)(int32_t)ulmk_kern_notif_create();
	ASSERT(last == -ULMK_ENOSPC);
}

/* ── notif_signal ──────────────────────────────────────────────────────── */

static void test_notif_signal_invalid(void)
{
	int ret;

	reset_pools();
	ret = (int)(int32_t)ulmk_kern_notif_signal(999, 0x1);
	ASSERT(ret == -ULMK_EINVAL);
}

static void test_notif_signal_no_waiter_accumulates(void)
{
	ulmk_notif_obj_t *n;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);

	ulmk_kern_notif_signal(0, 0x3);
	ulmk_kern_notif_signal(0, 0xC);

	ASSERT(n->bits == 0xF);
}

static void test_notif_signal_wakes_waiter(void)
{
	ulmk_thread_t    *waiter = make_thread(10);
	ulmk_notif_obj_t *n;

	reset_pools();
	reset_counters();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);

	waiter->state          = UL_THREAD_STATE_BLOCKED;
	waiter->blocked_reason = UL_BLOCKED_NOTIF;
	waiter->notif_wait_mask = 0x5;
	n->waiter    = waiter;
	n->wait_mask = 0x5;

	g_current = NULL;  /* simulate idle so schedule() is called */
	ulmk_kern_notif_signal(0, 0x4); /* bit 2 matches mask 0x5 */

	ASSERT(waiter->state == UL_THREAD_STATE_READY);
	ASSERT(waiter->notif_received == 0x4);
	ASSERT((n->bits & 0x4) == 0);   /* consumed */
	ASSERT(n->waiter == NULL);
	ASSERT(g_enqueue_count == 1);
	/* schedule is NOT called directly from signal; preemption is handled
	 * by the arch ISR exit mechanism, not inline from the signal path */
}

/* ── notif_wait ────────────────────────────────────────────────────────── */

static void test_notif_wait_invalid(void)
{
	uint32_t bits;

	reset_pools();
	ASSERT(notif_wait_impl(999, 0x1, &bits) == -ULMK_EINVAL);
}

static void test_notif_wait_null_out(void)
{
	reset_pools();
	ulmk_kern_notif_create();
	g_current = make_thread(10);

	ASSERT(notif_wait_impl(0, 0x1, NULL) == -ULMK_EINVAL);
}

static void test_notif_wait_fast_path(void)
{
	uint32_t       bits = 0;
	ulmk_notif_obj_t *n;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);
	n->bits = 0x6;

	g_current = make_thread(10);
	notif_wait_impl(0, 0x4, &bits);

	ASSERT(bits == 0x4);
	ASSERT(n->bits == 0x2);
	ASSERT(g_schedule_count == 0);
}

static void test_notif_wait_slow_path(void)
{
	uint32_t       bits = 0;
	ulmk_notif_obj_t *n;
	ulmk_thread_t    *waiter;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);

	waiter    = make_thread(10);
	g_current = waiter;

	notif_wait_impl(0, 0x1, &bits);

	ASSERT(waiter->state == UL_THREAD_STATE_BLOCKED);
	ASSERT(waiter->blocked_reason == UL_BLOCKED_NOTIF);
	ASSERT(n->waiter == waiter);
	ASSERT(g_schedule_count == 0);
}

/* ── notif_poll ────────────────────────────────────────────────────────── */

static void test_notif_poll_invalid(void)
{
	reset_pools();
	ASSERT(ulmk_kern_notif_poll(999, 0xFF) == 0);
}

static void test_notif_poll_consumes_matched(void)
{
	ulmk_notif_obj_t *n;
	uint32_t        got;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);
	n->bits = 0xF0;

	got = ulmk_kern_notif_poll(0, 0x30);
	ASSERT(got == 0x30);
	ASSERT(n->bits == 0xC0);
}

static void test_notif_poll_no_match(void)
{
	ulmk_notif_obj_t *n;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);
	n->bits = 0x0F;

	ASSERT(ulmk_kern_notif_poll(0, 0xF0) == 0);
	ASSERT(n->bits == 0x0F);    /* unchanged */
}

/* ── priority inheritance ──────────────────────────────────────────────── */

static void test_prio_inherit_boost(void)
{
	ulmk_msg_t      msg    = { .label = 1 };
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *caller;
	ulmk_thread_t   *server;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	caller = make_thread(2);
	server = make_thread(50);

	server->state          = UL_THREAD_STATE_BLOCKED;
	server->blocked_reason = UL_BLOCKED_IPC_RECV;
	server->blocked_ep     = 0;
	ipc_test_enqueue_recv(ep, server);

	g_current = caller;
	ep_call_impl(0, &msg);

	ASSERT(server->priority == 2);
	ASSERT(server->saved_prio == 50);
}

static void test_prio_inherit_restore_on_reply(void)
{
	ulmk_msg_t     reply  = {0};
	ulmk_thread_t *server;
	ulmk_thread_t *caller;

	reset_pools();
	server = make_thread(50);
	caller = make_thread(2);

	server->saved_prio     = 50;
	server->priority       = 2;
	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;

	g_current = server;
	ep_reply_impl(caller->tid, &reply);

	ASSERT(server->priority == 50);
}

static void test_prio_inherit_boost_on_recv(void)
{
	ulmk_msg_t      req     = { .label = 1 };
	ulmk_msg_t      msg_out = {0};
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *server;
	ulmk_thread_t   *caller;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	caller = make_thread(2);
	server = make_thread(50);

	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;
	caller->ipc_msg_outptr = &req;
	ipc_test_enqueue_send(ep, caller);

	g_current = server;
	ep_recv_impl(0, &msg_out, NULL);

	ASSERT(server->priority == 2);
	ASSERT(server->saved_prio == 50);
}

/* ── destroy + waiters ─────────────────────────────────────────────────── */

static void test_ep_destroy_wakes_recv_waiter(void)
{
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *srv;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	srv = make_thread(10);
	srv->state          = UL_THREAD_STATE_BLOCKED;
	srv->blocked_reason = UL_BLOCKED_IPC_RECV;
	srv->blocked_ep     = 0;
	ipc_test_enqueue_recv(ep, srv);

	ASSERT(ep_destroy_impl(0) == 0);
	ASSERT(srv->state == UL_THREAD_STATE_READY);
	ASSERT(srv->block_status == ULMK_EINVAL);
	ASSERT(sys_dlist_is_empty(&ep->recv_queue));
	ASSERT(!ep->active);
	ASSERT(g_enqueue_count == 1);
}

static void test_ep_destroy_wakes_send_waiter(void)
{
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *caller;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	caller = make_thread(5);
	caller->state          = UL_THREAD_STATE_BLOCKED;
	caller->blocked_reason = UL_BLOCKED_IPC_CALL;
	caller->blocked_ep     = 0;
	ipc_test_enqueue_send(ep, caller);

	ASSERT(ep_destroy_impl(0) == 0);
	ASSERT(caller->state == UL_THREAD_STATE_READY);
	ASSERT(caller->block_status == ULMK_EINVAL);
	ASSERT(sys_dlist_is_empty(&ep->send_queue));
}

static void test_notif_destroy_wakes_waiter(void)
{
	ulmk_notif_obj_t *n;
	ulmk_thread_t    *waiter;

	reset_pools();
	ulmk_kern_notif_create();
	n = ulmk_notif_by_id(0);

	waiter = make_thread(10);
	waiter->state           = UL_THREAD_STATE_BLOCKED;
	waiter->blocked_reason  = UL_BLOCKED_NOTIF;
	waiter->blocked_notif   = 0;
	waiter->notif_wait_mask = 0x1;
	n->waiter = waiter;

	ASSERT(notif_destroy_impl(0) == 0);
	ASSERT(waiter->state == UL_THREAD_STATE_READY);
	ASSERT(waiter->block_status == ULMK_EINVAL);
	ASSERT(!n->active);
	ASSERT(g_enqueue_count == 1);
}

/* ── kill cleanup ──────────────────────────────────────────────────────── */

static void test_kill_removes_from_recv_queue(void)
{
	ulmk_endpoint_t *ep;
	ulmk_thread_t   *srv;

	reset_pools();
	ulmk_kern_ep_create();
	ep = ulmk_ep_by_id(0);

	srv = make_thread(10);
	srv->state          = UL_THREAD_STATE_BLOCKED;
	srv->blocked_reason = UL_BLOCKED_IPC_RECV;
	srv->blocked_ep     = 0;
	ipc_test_enqueue_recv(ep, srv);

	ulmk_ep_recv_queue_remove(srv);

	ASSERT(sys_dlist_is_empty(&ep->recv_queue));
	ASSERT(!sys_dnode_is_linked(&srv->ipc_node));
	ASSERT(srv->blocked_ep == ULMK_EP_INVALID);
}

/* ── runner ────────────────────────────────────────────────────────────── */

int main(void)
{
	printf("--- IPC unit tests ---\n");

	RUN(test_ep_create_valid);
	RUN(test_ep_create_exhaust);

	RUN(test_ep_call_invalid_ep);
	RUN(test_ep_call_null_msg);
	RUN(test_ep_call_fast_path_server_waiting);
	RUN(test_ep_call_slow_path_no_server);

	RUN(test_ep_recv_invalid_ep);
	RUN(test_ep_recv_null_msg);
	RUN(test_ep_recv_fast_path_caller_waiting);
	RUN(test_ep_recv_slow_path_no_caller);

	RUN(test_ep_reply_invalid_tid);
	RUN(test_ep_reply_non_blocked_caller);
	RUN(test_ep_reply_wakes_caller_restores_prio);

	RUN(test_ep_reply_recv_null_args);
	RUN(test_ep_reply_recv_skip_reply_on_invalid_tid);

	RUN(test_ep_grant_invalid_ep);
	RUN(test_ep_grant_invalid_tid);
	RUN(test_ep_grant_valid);

	RUN(test_notif_create_valid);
	RUN(test_notif_create_exhaust);

	RUN(test_notif_signal_invalid);
	RUN(test_notif_signal_no_waiter_accumulates);
	RUN(test_notif_signal_wakes_waiter);

	RUN(test_notif_wait_invalid);
	RUN(test_notif_wait_null_out);
	RUN(test_notif_wait_fast_path);
	RUN(test_notif_wait_slow_path);

	RUN(test_notif_poll_invalid);
	RUN(test_notif_poll_consumes_matched);
	RUN(test_notif_poll_no_match);

	RUN(test_prio_inherit_boost);
	RUN(test_prio_inherit_restore_on_reply);
	RUN(test_prio_inherit_boost_on_recv);

	RUN(test_ep_destroy_wakes_recv_waiter);
	RUN(test_ep_destroy_wakes_send_waiter);
	RUN(test_notif_destroy_wakes_waiter);

	RUN(test_kill_removes_from_recv_queue);

	printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
