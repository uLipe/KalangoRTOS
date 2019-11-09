#include <errno.h>
#include <stddef.h>
#include <KalangoRTOS/priority_queue.h>

#ifdef __GNUC__
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

static inline void detach_node(struct priority_queue *pq, struct heap_node *n)
{
    if (n->prev) n->prev->next = n->next;
    else         pq->head = n->next;

    if (n->next) n->next->prev = n->prev;
    else         pq->tail = n->prev;

    if (pq->root == n) {
        pq->root = pq->head; 
    }

    n->prev = n->next = n->parent = NULL;
}

int pq_init(struct priority_queue *pq,
            int (*compare)(struct heap_node *, struct heap_node *))
{
    if (unlikely(!pq || !compare)) return -EINVAL;

    pq->head = NULL;
    pq->tail = NULL;
    pq->root = NULL;
    pq->compare = compare;
    return 0;
}

int pq_insert(struct priority_queue *pq, struct heap_node *node)
{
    if (unlikely(!pq || !node)) return -EINVAL;

    node->parent = NULL;
    node->next = NULL;
    node->prev = pq->tail;

    if (!pq->head) {
        pq->head = pq->tail = pq->root = node;
        return 0;
    }

    pq->tail->next = node;
    pq->tail = node;

    return 0;
}
int pq_remove(struct priority_queue *pq, struct heap_node *node)
{
    if (unlikely(!pq || !node)) return -EINVAL;
    detach_node(pq, node);

    if (!pq->head) pq->root = NULL;
    return 0;
}

struct heap_node *pq_peek(struct priority_queue *pq)
{
    if (unlikely(!pq)) return NULL;
    return pq->root;
}

struct heap_node *pq_pop(struct priority_queue *pq)
{
    if (unlikely(!pq || !pq->root)) return NULL;

    struct heap_node *n = pq->root;
    detach_node(pq, n);

    pq->root = pq->head;

    return n;
}


static inline struct heap_node *split_run(struct heap_node *head, size_t run_len)
{
    while (head && run_len--) {
        head = head->next;
    }
    if (!head) return NULL;

    struct heap_node *next = head;
    struct heap_node *prev = next->prev;
    if (prev) prev->next = NULL;
    next->prev = NULL;
    return next;
}

static struct heap_node *merge_runs(struct priority_queue *pq,
                                    struct heap_node *a,
                                    struct heap_node *b,
                                    struct heap_node **out_tail)
{
    struct heap_node *head = NULL;
    struct heap_node *tail = NULL;

    while (a && b) {
        struct heap_node *pick;
        if (pq->compare(a, b) >= 0) {
            pick = a; a = a->next;
        } else {
            pick = b; b = b->next;
        }

        pick->prev = tail;
        if (tail) tail->next = pick;
        else      head = pick;
        tail = pick;
    }

    struct heap_node *rest = a ? a : b;
    while (rest) {
        struct heap_node *pick = rest;
        rest = rest->next;

        pick->prev = tail;
        if (tail) tail->next = pick;
        else      head = pick;
        tail = pick;
    }

    if (tail) tail->next = NULL;
    *out_tail = tail;
    return head;
}

void pq_reorder(struct priority_queue *pq)
{
    if (unlikely(!pq || !pq->head || !pq->head->next)) {
        if (pq && pq->head) pq->root = pq->head;
        return;
    }

    // Bottom-up mergesort: run = 1,2,4,8...
    size_t run_len = 1;

    while (1) {
        struct heap_node *cur = pq->head;
        struct heap_node *new_head = NULL;
        struct heap_node *new_tail = NULL;
        size_t merges = 0;

        while (cur) {
            merges++;

            struct heap_node *left  = cur;
            struct heap_node *right = split_run(left, run_len);
            cur = split_run(right, run_len);

            struct heap_node *merged_tail = NULL;
            struct heap_node *merged_head = merge_runs(pq, left, right, &merged_tail);

            if (!new_head) {
                new_head = merged_head;
                new_tail = merged_tail;
            } else {
                merged_head->prev = new_tail;
                new_tail->next = merged_head;
                new_tail = merged_tail;
            }
        }

        pq->head = new_head;
        pq->tail = new_tail;
        pq->root = pq->head;

        if (merges <= 1) break;
        run_len <<= 1;
    }
}
