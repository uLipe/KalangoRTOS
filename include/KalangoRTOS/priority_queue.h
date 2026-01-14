/*
 * MIT License
 * 
 * Copyright (c) 2025 Felipe Neves
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

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stddef.h>

/**
 * struct heap_node - Intrusive heap node structure
 * @next: Pointer to the next node
 * @prev: Pointer to the previous node
 * @parent: Pointer to the parent node
 */
struct heap_node {
    struct heap_node *next;
    struct heap_node *prev;
    struct heap_node *parent;
};

/**
 * struct priority_queue - Priority queue structure
 * @head: Pointer to the head of the queue
 * @tail: Pointer to the tail of the queue
 * @root: Pointer to the root of the heap
 * @compare: Function pointer to the comparison function
 */
struct priority_queue {
    struct heap_node *head;
    struct heap_node *tail;
    struct heap_node *root;
    int (*compare)(struct heap_node *, struct heap_node *);
};

/**
 * pq_init - Initialize the priority queue
 * @pq: Pointer to the priority queue
 * @compare: Comparison function for heap nodes
 *
 * Return: 0 on success, -EINVAL on invalid parameters
 */
int pq_init(struct priority_queue *pq, int (*compare)(struct heap_node *, struct heap_node *));

/**
 * pq_insert - Insert a node into the priority queue without reordering
 * @pq: Pointer to the priority queue
 * @node: Pointer to the node to be inserted
 *
 * Return: 0 on success, -EINVAL on invalid parameters
 */
int pq_insert(struct priority_queue *pq, struct heap_node *node);

/**
 * pq_pop - Remove and return the highest-priority node
 * @pq: Pointer to the priority queue
 *
 * Return: Pointer to the highest-priority node, or NULL on failure
 */
struct heap_node *pq_pop(struct priority_queue *pq);

/**
 * pq_peek - Retrieve the highest-priority node without removing it
 * @pq: Pointer to the priority queue
 *
 * Return: Pointer to the highest-priority node (root of the heap), or NULL if the queue is empty.
 */
struct heap_node *pq_peek(struct priority_queue *pq);


/**
 * pq_reorder - Reorder the priority queue to maintain the heap property
 * @pq: Pointer to the priority queue
 */
void pq_reorder(struct priority_queue *pq);

#endif /* PRIORITY_QUEUE_H */

