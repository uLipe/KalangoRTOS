/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Syscall numbers — ulipeMicroKernel
 * Single source of truth for both userspace wrappers and the kernel router.
 * Full specification: docs/api_spec.md §12
 *
 * Numbering rationale (sparse on purpose — room for additions per group):
 *   1–9   Memory
 *   10–19 Scheduling / exit
 *   20–29 Thread query (unprivileged)
 *   30–39 IPC endpoints
 *   40–49 Notifications
 *   60–69 IRQ (IO >= 1)
 *   70–79 Thread management (IO >= 1)
 *   80–89 Process management (IO >= 1)
 */

#ifndef UL_SYSCALL_NR_H
#define UL_SYSCALL_NR_H

/* ── Memory ──────────────────────────────────────────────────────── */
#define UL_SYS_MMAP                 1  /* void *ul_mem_map(hint, sz, perms, flags) */
#define UL_SYS_MUNMAP               2  /* int   ul_mem_unmap(addr, sz)            */
#define UL_SYS_MEM_GRANT            3  /* int   ul_mem_grant(addr, sz, tid, perms)*/

/* ── Scheduling / exit ───────────────────────────────────────────── */
#define UL_SYS_YIELD               10  /* void  ul_thread_yield(void)             */
#define UL_SYS_EXIT                11  /* void  ul_thread_exit(void) [noreturn]   */

/* ── Thread query (any privilege) ───────────────────────────────── */
#define UL_SYS_THREAD_SELF         20  /* ul_tid_t ul_thread_self(void)           */

/* ── IPC endpoints ───────────────────────────────────────────────── */
#define UL_SYS_EP_CREATE           30  /* ul_ep_t ul_ep_create(void)              */
#define UL_SYS_EP_CALL             31  /* int     ul_ep_call(ep, msg*)            */
#define UL_SYS_EP_RECV             32  /* int     ul_ep_recv(ep, msg*, sender*)   */
#define UL_SYS_EP_REPLY            33  /* int     ul_ep_reply(sender, reply*)     */
#define UL_SYS_EP_REPLY_RECV       34  /* int     ul_ep_reply_recv(...)           */
#define UL_SYS_EP_GRANT            35  /* int     ul_ep_grant(ep, target)         */
#define UL_SYS_EP_RECV_OR_NOTIF    36  /* int     ul_ep_recv_or_notif(...)        */

/* ── Notifications ───────────────────────────────────────────────── */
#define UL_SYS_NOTIF_CREATE        40  /* ul_notif_t ul_notif_create(void)        */
#define UL_SYS_NOTIF_SIGNAL        41  /* int        ul_notif_signal(notif, bits) */
#define UL_SYS_NOTIF_WAIT          42  /* int        ul_notif_wait(notif, mask, bits*) */
#define UL_SYS_NOTIF_POLL          43  /* uint32_t   ul_notif_poll(notif, mask)   */

/* ── IRQ (requires IO >= 1 / UL_PRIV_DRIVER) ────────────────────── */
#define UL_SYS_IRQ_BIND            60  /* int ul_irq_bind(srpn, notif, bit)       */
#define UL_SYS_IRQ_ENABLE          61  /* int ul_irq_enable(srpn)                 */
#define UL_SYS_IRQ_DISABLE         62  /* int ul_irq_disable(srpn)                */
#define UL_SYS_IRQ_ACK             63  /* int ul_irq_ack(srpn)                    */

/* ── Thread management (requires IO >= 1 / UL_PRIV_DRIVER) ─────── */
#define UL_SYS_THREAD_SPAWN        70  /* ul_tid_t ul_thread_create(attr*)        */
#define UL_SYS_THREAD_KILL         71  /* int      ul_thread_kill(tid)            */
#define UL_SYS_THREAD_SUSPEND      72  /* int      ul_thread_suspend(tid)         */
#define UL_SYS_THREAD_RESUME       73  /* int      ul_thread_resume(tid)          */
#define UL_SYS_THREAD_SET_PRIO     74  /* int      ul_thread_priority_set(tid,p)  */
#define UL_SYS_THREAD_GET_PRIO     75  /* int      ul_thread_priority_get(tid)    */

/* ── Process management (requires IO >= 1 / UL_PRIV_DRIVER) ─────── */
#define UL_SYS_PROC_CREATE         80
#define UL_SYS_PROC_DESTROY        81
#define UL_SYS_PROC_ADD_REGION     82
#define UL_SYS_PROC_GRANT_CAP      83
#define UL_SYS_PROC_GRANT_IRQ      84

/* Upper bound used by the router for range validation. */
#define UL_SYS_MAX                128

#endif /* UL_SYSCALL_NR_H */
