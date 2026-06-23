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

#include "include/ul_sched.h"

#define HARD_TOP_PRIORITY       32

static struct {
    int locked;
    uint32_t ready_prio_bitmap;
    struct ul_dlist ready_threads[HARD_TOP_PRIORITY];
} ul_run_queue;

static struct ul_thread* high_prio_ready_thread = NULL;

int ul_sched_init(void)
{
    ul_run_queue.locked = 0;
    ul_run_queue.ready_prio_bitmap = 0;
    for(int i = 0; i < HARD_TOP_PRIORITY; i++) {
        ul_dlist_init(&ul_run_queue.ready_threads[i]);
    }
    return 0;
}

int ul_sched_add(struct ul_thread *th)
{
    int reesched = 0;

    if(!th)
        return -EINVAL;

    int key = ul_arch_irq_lock();
    ul_dlist_append(&ul_run_queue.ready_threads[th->priority], &th->rdy_q_link);
    ul_run_queue.ready_prio_bitmap |= (1 << th->priority);
    th->scheduled = true;
    ul_arch_irq_unlock(key);

    return 0;
}

int ul_sched_remove(struct ul_thread *th)
{
    if(!th)
        return -EINVAL;

    int key = ul_arch_irq_lock();
    ul_dlist_remove_node(&th->rdy_q_link);
    if(ul_dlist_is_empty(&ul_run_queue.ready_threads[th->priority])) {
        ul_run_queue.ready_prio_bitmap &= ~(1 << th->priority);
    }
    th->scheduled = false;
    ul_arch_irq_unlock(key);

    return 0;
}

int ul_sched_lock(void)
{
    int key = ul_arch_irq_lock();
    int r = (ul_run_queue.locked == 0xFFF) ? EFAULT : 0;
    if(!r)
        ul_run_queue.locked++;
    ul_arch_irq_unlock(key);

    return r;
}

int ul_sched_unlock(void)
{
    int key = ul_irq_lock();
    ul_run_queue.locked = (ul_run_queue.locked > 0) ? ul_run_queue.locked - 1 : 0;
    ul_arch_irq_unlock(key);

    return 0;
}

struct ul_thread *ul_do_sched(void)
{
    /* If scheduler is locked just return the task obtained in the last scheduling*/
    int locked = ul_run_queue.locked;
    if(locked)
        return high_prio_ready_thread;

    int key = ul_arch_irq_lock();
    int prio = ul_arch_count_lead_zeros(&ul_run_queue.ready_prio_bitmap);
    if(!prio && ul_dlist_is_empty(&ul_run_queue.ready_threads[0])) {
        high_prio_ready_thread = NULL;
    } else {
        ul_dnode_t *node = ul_dlist_peek_head(&ul_run_queue.ready_threads[prio])
        high_prio_ready_thread = UL_CONTAINER_OF(node, struct ul_thread, rdy_q_link);
    }

    ul_arch_irq_unlock(key);
    return high_prio_ready_thread;
}
