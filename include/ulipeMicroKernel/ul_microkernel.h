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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "microkernel/ul_kernel_structures.h"
#include "microkernel/ul_microkernel_ipc_comands.h"

int ul_kthread_create(struct ul_thread *th, thread_function_t f, void *arg, int priority, void *thread_stack, int stack_size);
int ul_kthread_suspend(struct ul_thread *th);
int ul_kthread_resume(struct ul_thread *th);
struct ul_thread *ul_kthread_get_current(void);
int ul_kthread_priority_set(struct ul_thread *th, int new_prio, int *old_prio);
int ul_kthread_priority_get(struct ul_thread *th);
int ul_kthread_yield(void);

int ul_kipc_create_channel(void);
int ul_kipc_receive(struct ul_ipc_msg *msg);
int ul_kipc_reply(int client_id, struct ul_ipc_msg *reply);
int ul_kipc_send(int channel_id, struct ul_ipc_msg *cmd, int wait_reply);

int ul_kisr_attach(int irq_number, ul_isr_handler_t fn, void *user_data);
int ul_kirq_enable(int irq_number);
int ul_kirq_disable(int irq_number);
int ul_kenter_critical(void);
int ul_kexit_critical(int key);
int ul_kraise_softirq(int irq_number);

int ul_kstart(void);
