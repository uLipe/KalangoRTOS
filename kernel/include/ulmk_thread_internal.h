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
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include <kernel/include/list.h>

#define UL_THREAD_STATE_DEAD      0
#define UL_THREAD_STATE_READY     1
#define UL_THREAD_STATE_RUNNING   2
#define UL_THREAD_STATE_BLOCKED   3
#define UL_THREAD_STATE_SUSPENDED 4

/* Reason a thread is in UL_THREAD_STATE_BLOCKED. */
#define UL_BLOCKED_NONE          0
#define UL_BLOCKED_IPC_CALL      2  /* waiting for server reply */
#define UL_BLOCKED_IPC_RECV      3  /* waiting for a caller */
#define UL_BLOCKED_NOTIF         4  /* waiting for notification bits */
#define UL_BLOCKED_IPC_OR_NOTIF  5  /* ulmk_ep_recv_or_notif — either */

typedef struct ulmk_thread {
	ulmk_arch_ctx_t    ctx;
	uint8_t         *stack_base;
	size_t           stack_size;
	/*
	 * slabAO — contiguous allocation (stack + heap) from user_pool.
	 * NULL for static threads (idle, root).  TCB is always a separate
	 * allocation so userspace DPR cannot reach kernel metadata.
	 */
	void            *slab_base;	/* base of slabAO allocation */
	size_t           slab_size;	/* stack_size + heap_size */
	uintptr_t        heap_base;	/* slab_base + stack_size */
	size_t           heap_size;	/* bytes reserved for thread heap */
	uint8_t          priority;
	uint8_t          saved_prio;      /* priority before inheritance boost */
	uint8_t          state;
	uint8_t          blocked_reason;
	ulmk_privilege_t   privilege;
	ulmk_tid_t         tid;
	ulmk_ep_t          blocked_ep;      /* ep blocked on; for cleanup on kill */
	ulmk_notif_t       blocked_notif;   /* notif blocked on; recv_or_notif */
	sys_dnode_t        sched_node;      /* run-queue linkage */
	sys_dnode_t        ipc_node;        /* IPC send or recv queue linkage */
	sys_dnode_t        reg_node;        /* global TCB registry linkage */
	ulmk_msg_t          ipc_msg;        /* in-flight message buffer */
	ulmk_tid_t          ipc_sender;     /* sender TID stored by recv for reply */
	/*
	 * Output pointers saved in the TCB before a blocking recv.  Written
	 * back after wakeup so the result survives the context restore on
	 * re-schedule.
	 */
	ulmk_msg_t         *ipc_msg_outptr;
	ulmk_tid_t         *ipc_sender_outptr;
	uint32_t         *notif_bits_outptr;
	ulmk_recv_or_notif_result_t *rn_result_outptr;
	uint32_t          notif_wait_mask;
	uint32_t          notif_received; /* bits consumed on notif wakeup */
	/* MPU regions owned by this thread (configured by mpu_switch on dispatch) */
	ulmk_arch_region_t  regions[ULMK_ARCH_MAX_REGIONS];
	uint8_t           region_count;
	/*
	 * Capability bitmask — which privileged operations this thread may invoke.
	 */
	uint8_t           cap_flags;
} ulmk_thread_t;

int          ulmk_thread_init(ulmk_thread_t *th, const ulmk_thread_attr_t *attr,
			    void *stack);
ulmk_thread_t *ulmk_thread_by_tid(ulmk_tid_t tid);
void         ulmk_thread_set_state(ulmk_thread_t *th, uint8_t state);
void         ulmk_thread_free(ulmk_thread_t *th);

#endif /* UL_THREAD_INTERNAL_H */
