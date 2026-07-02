/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal thread management types and interface.
 * Not part of the public API — do not include from userspace.
 */

#ifndef UL_THREAD_INTERNAL_H
#define UL_THREAD_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <ul/microkernel.h>
#include <ul_arch.h>

#define UL_THREAD_STATE_DEAD      0
#define UL_THREAD_STATE_READY     1
#define UL_THREAD_STATE_RUNNING   2
#define UL_THREAD_STATE_BLOCKED   3
#define UL_THREAD_STATE_SUSPENDED 4

/* Reason a thread is in UL_THREAD_STATE_BLOCKED. */
#define UL_BLOCKED_NONE          0
#define UL_BLOCKED_TIMER_WAIT    1  /* waiting for hardware timer deadline */
#define UL_BLOCKED_IPC_CALL      2  /* waiting for server reply */
#define UL_BLOCKED_IPC_RECV      3  /* waiting for a caller */
#define UL_BLOCKED_NOTIF         4  /* waiting for notification bits */
#define UL_BLOCKED_IPC_OR_NOTIF  5  /* ul_ep_recv_or_notif — either */

typedef struct ul_thread {
	ul_arch_ctx_t    ctx;
	uint8_t         *stack_base;
	size_t           stack_size;
	uint8_t          priority;
	uint8_t          saved_prio;      /* priority before inheritance boost */
	uint8_t          state;
	uint8_t          blocked_reason;
	ul_privilege_t   privilege;
	ul_tid_t         tid;
	ul_ep_t          blocked_ep;      /* ep blocked on; for cleanup on kill */
	ul_notif_t       blocked_notif;   /* notif blocked on; recv_or_notif */
	struct ul_thread *next;           /* run-queue forward linkage */
	struct ul_thread *sched_prev;     /* run-queue backward linkage (O(1) dequeue) */
	struct ul_thread *ipc_next;       /* IPC send/recv queue linkage */
	struct ul_thread *reg_next;       /* global TCB registry linkage */
	ul_msg_t          ipc_msg;        /* in-flight message buffer */
	ul_tid_t          ipc_sender;     /* sender TID stored by recv for reply */
	/*
	 * Output pointers saved in the TCB before a blocking recv.  Written
	 * back after wakeup so the result survives the context restore on
	 * re-schedule.
	 */
	ul_msg_t         *ipc_msg_outptr;
	ul_tid_t         *ipc_sender_outptr;
	uint32_t         *notif_bits_outptr;
	ul_recv_or_notif_result_t *rn_result_outptr;
	uint32_t          notif_wait_mask;
	uint32_t          notif_received; /* bits consumed on notif wakeup */
	/* MPU regions owned by this thread (configured by mpu_switch on dispatch) */
	ul_arch_region_t  regions[UL_ARCH_MAX_REGIONS];
	uint8_t           region_count;
	/* Preemption time-slice countdown (scheduler ticks). Reset on dispatch. */
	uint32_t          ticks_remaining;
	/*
	 * Capability bitmask — which privileged operations this thread may invoke.
	 */
	uint8_t           cap_flags;
} ul_thread_t;

int          ul_thread_init(ul_thread_t *th, const ul_thread_attr_t *attr,
			    void *stack);
ul_thread_t *ul_thread_by_tid(ul_tid_t tid);
void         ul_thread_set_state(ul_thread_t *th, uint8_t state);
void         ul_thread_free(ul_thread_t *th);

#endif /* UL_THREAD_INTERNAL_H */
