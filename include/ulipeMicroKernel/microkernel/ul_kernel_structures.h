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

#include "ul_list.h"
#include "ul_util.h"

struct ul_ipc_msg {
    uint32_t code;
    uint64_t contents;
    ul_dnode_t link;
};

struct ul_ipc_handle {
    int is_server;
    int old_server_prio;
    struct ul_dlist msgs;
};

struct ul_wait_queue {
    struct ul_dlist list;
}

struct ul_thread {
    void *stack_top;
    void *stack_base;
    int stack_size;
    thread_function_t function;
    void *arg;
    int priority;
    int suspension_nesting;
    int64_t next_wakeup;
    ul_dnode_t rdy_q_link;
    ul_dnode_t wait_q_link;
    bool scheduled;
    struct ul_ipc_handle ipc_handle;
};

typedef void (ul_isr_handler_t)(void *user_data);