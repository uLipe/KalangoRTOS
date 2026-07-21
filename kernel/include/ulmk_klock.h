/* SPDX-License-Identifier: MIT */
/*
 * Global kernel spinlocks for shared pools (ep/notif/thread/mem/irq).
 *
 * Lock order (never invert): thread → ipc (ep/notif) → rq → irq → timer → mem
 * RQ lock is internal to bitmap_rt (enqueue/dequeue/pick/peek).
 */

#ifndef UL_KLOCK_H
#define UL_KLOCK_H

#include <ulmk_arch.h>

extern ulmk_spinlock_t g_ulmk_lock_thread;
extern ulmk_spinlock_t g_ulmk_lock_ipc;	/* ep + notif (shared for recv_or_notif) */
extern ulmk_spinlock_t g_ulmk_lock_irq;
extern ulmk_spinlock_t g_ulmk_lock_timer;
extern ulmk_spinlock_t g_ulmk_lock_mem;

#endif /* UL_KLOCK_H */
