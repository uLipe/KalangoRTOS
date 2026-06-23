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

#include <ulipeMicroKernel/ul_microkernel.h>
#include "include/ul_arch.h"
#include "include/ul_sched.h"
#include "include/ul_wait_queue.h"

#define IRQ_THREAD_PRIORITY     31
#define IDLE_THREAD_PRIORITY    0

#define USER_PRIORITY_MAX       30
#define USER_PRIORITY_MIN       1
#define USER_THREAD_DEFAULT_PRIORITY USER_PRIORITY_MAX
#define MINIMAL_STACK_SIZE_BYTES   256

static struct ul_thread *current = NULL;
static struct ul_thread *high_prio_ready_thread = NULL;
static struct ul_thread idle_thread;
static struct ul_thread irq_thread;
static struct ul_thread user_thread;
static uint8_t idle_stack[512];
static uint8_t irq_stack[2048];
static uint8_t user_stack[4096];
static struct ul_dlist suspended_threads;
static int isr_channel = &irq_thread.ipc_handle;

static void isr_server_thread(void *arg)
{
    struct ul_ipc_msg req;
    int next_irq = -1;

    /* ISR Thread is a special case of IPC. so create its channel manually */
    ul_sched_lock();
    current->ipc_handle.is_server = 1;
    ul_dlist_init(&current->ipc_handle.msgs);
    current->ipc_handle.old_server_prio = current->priority;
    ul_sched_unlock();

    while(1) {
        ul_kipc_receive(&req);
        next_irq = ul_arch_get_next_pending_isr();
        while(next_irq > 0) {
            ul_arch_dispatch_isr(next_irq);
        }
    }
}

static void idle_server_thread(void *arg)
{
    while(1) {
        ul_arch_idle();
    }
}

static bool ul_inside_isr(void)
{
    return (bool)(current == &irq_thread);
}

static int ul_kthread_create_ex(struct ul_thread *th, thread_function_t f, void *arg, int priority, void *thread_stack, int stack_size)
{
    ASSERT_PARAM(th != NULL);
    ASSERT_PARAM(f != NULL);
    ASSERT_PARAM(thread_stack != NULL);
    ASSERT_PARAM(stack_size >= MINIMAL_STACK_SIZE_BYTES);

    th->stack_base = thread_stack;
    th->stack_size = stack_size;
    th->function = f;
    th->arg = arg;
    th->priority = priority;
    th->suspension_nesting = 0;
    th->sleep_ticks
    ul_dlist_init(&th->ipc_handle.msgs);
    th->ipc_handle.is_server = 0;
    ul_arch_new_thread(th);
    ul_sched_add(th);

    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    return 0;
}

int ul_kthread_create(struct ul_thread *th, thread_function_t f, void *arg, int priority, void *thread_stack, int stack_size)
{
    ASSERT_PARAM((priority >= USER_PRIORITY_MIN) && (priority <= USER_PRIORITY_MAX));
    return ul_kthread_create_ex(th, f, arg, priority, thread_stack, stack_size);
}

int ul_kthread_suspend(struct ul_thread *th)
{
    if(th == NULL) {
        th = ul_thread_get_current();
    }

    ASSERT_KERNEL(th != NULL);
    ASSERT_KERNEL(!ul_inside_isr());

    if(th->suspension_nesting > 0) {
        th->suspension_nesting++;
        return EALREADY;
    }

    ul_sched_lock();
    ul_sched_remove(th);
    ul_dlist_append(&suspended_threads, &th->wait_q_link);
    th->suspension_nesting++;
    ul_sched_unlock();

    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    return 0;
}

int ul_kthread_resume(struct ul_thread *th)
{
    ASSERT_PARAM(th != NULL);

    if(!th->suspension_nesting)
        return -EINVAL;

    th->suspension_nesting--;
    if(th->suspension_nesting) {
        return EALREADY;
    }

    ul_sched_lock();
    ul_dlist_remove_node(&th->wait_q_link);
    ul_sched_add(th);
    ul_sched_unlock();

    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    return 0;
}

struct ul_thread *ul_kthread_get_current(void)
{
    const struct ul_thread *th = current;
    return current;
}

int ul_kthread_priority_set(struct ul_thread *th, int new_prio, int *old_prio)
{
    ASSERT_PARAM(old_prio);
    ASSERT_PARAM((new_prio >= USER_PRIORITY_MIN) && (new_prio <= USER_PRIORITY_MAX));

    if(th == NULL) {
        th = ul_thread_get_current();
    }

    ASSERT_KERNEL(th != NULL);

    int prio = th->priority;
    th->priority = new_prio;

    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    *old_prio = prio;
    return 0;
}

int ul_kthread_priority_get(struct ul_thread *th)
{
    int prio = 0;

    if(th == NULL) {
        th = ul_thread_get_current();
    }

    ASSERT_KERNEL(th != NULL);
    prio = th->priority;
    return prio;
}

int ul_kthread_yield(void)
{
    ASSERT_KERNEL(!ul_inside_isr());

    ul_sched_lock();
    ul_sched_remove(current);
    ul_sched_add(current);
    ul_sched_unlock();
    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    return 0;
}

int ul_kipc_create_channel(void)
{
    ASSERT_KERNEL(!ul_inside_isr());

    ul_sched_lock();
    current->ipc_handle.is_server = 1;
    ul_dlist_init(&current->ipc_handle.msgs);
    current->ipc_handle.old_server_prio = current->priority;
    ul_sched_unlock();

    return (int)(&current->ipc_handle);
}

int ul_kipc_receive(struct ul_ipc_msg *msg)
{
    ASSERT_PARAM(msg);
    ASSERT_KERNEL(current->ipc_handle.is_server != 0);
    ASSERT_KERNEL(!ul_inside_isr());

    ul_sched_lock();
    if(ul_dlist_is_empty(&current->ipc_handle.msgs)) {
        ul_sched_remove(current);
        ul_sched_unlock();
        high_prio_ready_thread = ul_do_sched();
        ul_arch_switch(current,high_prio_ready_thread);
        ul_sched_lock();
    }

    struct ul_ipc_msg *from_kernel = CONTAINER_OF(
                                        ul_dlist_peek_head(&current->ipc_handle.msgs),
                                        struct ul_ipc_msg,
                                        link );

    ul_dlist_remove_node(&from_kernel->link);

    current->priority = current->ipc_handle.old_server_prio;
    ul_sched_remove(current);
    ul_sched_unlock();
    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    *msg = *from_kernel;
    return 0;
}

int ul_kipc_send(int channel_id, struct ul_ipc_msg *cmd, int wait_reply)
{
    int old_server_prio = 0;
    struct ul_ipc_handle *channel = (struct ul_ipc_handle *)channel_id;
    struct ul_thread *server = CONTAINER_OF(channel, struct ul_thread, ipc_handle);

    ASSERT_PARAM(cmd != NULL);
    ASSERT_PARAM(channel != NULL);
    ASSERT_KERNEL(channel->is_server != 0);
    ASSERT_KERNEL(current->ipc_handle.is_server == 0);
    ASSERT_PARAM((wait_reply && !ul_inside_isr()));

    ul_sched_lock();
    ul_dlist_append(&channel->msgs, &cmd->link);

    if(current->priority > server->priority) {
        server->priority = current->priority;
    }

    if(wait_reply) {
        ul_sched_remove(current);
    }

    ul_sched_add(server);
    ul_sched_unlock();

    high_prio_ready_thread = ul_do_sched();
    ul_arch_switch(current,high_prio_ready_thread);

    return 0;
}

int ul_kipc_reply(int client_id, struct ul_ipc_msg *reply)
{
    ASSERT_PARAM(reply);
    ASSERT_KERNEL(current->ipc_handle.is_server);
    ASSERT_KERNEL(!ul_inside_isr());

    struct ul_thread *client = (struct ul_thread *)client_id;
    ASSERT_KERNEL(client->ipc_handle.is_server == 0);

    ul_sched_lock();
    ul_dlist_append(&client->ipc_handle.msgs, &reply->link);
    ul_sched_add(client);
    ul_sched_unlock();

    if(client->priority > current->priority) {
        high_prio_ready_thread = ul_do_sched();
        ul_arch_switch(current,high_prio_ready_thread);
    }

    return 0;
}

int ul_kisr_attach(int irq_number, ul_isr_handler_t fn, , void *user_data)
{
    int ret;
    ul_sched_lock();
    ret = ul_arch_install_isr(irq_number, fn, user_data);
    ul_sched_unlock();
    return ret;
}

int ul_kirq_enable(int irq_number)
{
    int ret;
    ul_sched_lock();
    ret = ul_arch_enable_irq(irq_number);
    ul_sched_unlock();
    return ret;
}

int ul_kirq_disable(int irq_number)
{
    int ret;
    ul_sched_lock();
    ret = ul_arch_disable_irq(irq_number);
    ul_sched_unlock();
    return ret;
}

int ul_kraise_softirq(int irq_number)
{
    struct ul_ipc_msg req;
    int ret;
    req.code = (uint32_t)irq_number;
    return (ul_kipc_send(isr_channel, &req, 0));
}

int ul_kenter_critical(void)
{
    return ul_arch_irq_lock();
}

int ul_kexit_critical(int key)
{
    return ul_arch_irq_unlock(key);
}

int ul_kstart(void)
{
    ul_arch_irq_lock();
    ul_dlist_init(&suspended_threads);
    ul_sched_init();

    current = &idle_thread;
    high_prio_ready_thread = &idle_thread;

    ul_kthread_create_ex(&irq_thread, irq_server_thread,
                    NULL,
                    IRQ_THREAD_PRIORITY,
                    &irq_stack,
                    sizeof(irq_stack));

    ul_kthread_create_ex(&idle_thread, idle_server_thread,
                    NULL,
                    IDLE_THREAD_PRIORITY,
                    &idle_stack,
                    sizeof(idle_stack));

    extern void user_root_thread(void *arg);

    ul_kthread_create_ex(&user_thread, user_root_thread,
                    &user_thread,
                    USER_THREAD_DEFAULT_PRIORITY,
                    &user_stack,
                    sizeof(user_stack));

    return ul_arch_kmicrokernel_start();
}