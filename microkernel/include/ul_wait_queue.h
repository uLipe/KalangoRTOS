/*
 * MIT License
 *
 * Copyright (c) 2024 Felipe Neves
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <ulipe-rtos/kernel/ul_kernel_structures.h>
#include <ulipe-rtos/kernel/ul_dlist.h>
#include <ulipe-rtos/kernel/ul_util.h>

#include "ul_arch.h"
#include "ul_sched.h"

static inline void ul_wait_queue_init(struct ul_wait_queue *wq)
{
    int key = ul_arch_irq_lock();
    ul_dlist_init(&wq->list);
    ul_arch_irq_unlock(key);
}

static inline void ul_wait_queue_put(struct ul_wait_queue *wq, struct ul_thread *th)
{
    int key = ul_arch_irq_lock();
    ul_dlist_append(&wq->list, &th->wait_q_link)
    ul_arch_irq_unlock(key);

    return 0;
}

static inline struct ul_thread *ul_wait_queue_get_next(struct ul_wait_queue *wq)
{
    int key = ul_arch_irq_lock();
    ul_dnode_t *node = ul_dlist_peek_head(&wq->list);
    struct ul_thread *th = NULL;

    if(node) {
        th = UL_CONTAINER_OF(node, struct ul_thread, wait_q_link);
    }

    ul_arch_irq_unlock(key);
    return th;
}

static inline ul_unpend_thread_from_waitq(struct ul_wait_queue *wq)
{
    struct ul_thread *next;
    int r;
    ul_sched_lock();
    next = ul_wait_queue_get_next(wq);
    if(next) {
        r = ul_sched_add(th);
    }
    ul_sched_unlock();
    return r;
}