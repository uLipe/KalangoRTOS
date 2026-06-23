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

#include <stdbool.h>

struct ul_dlist {
    struct ul_dlist *prev;
    struct ul_dlist *next;
};

typedef struct ul_dlist ul_dnode_t;

static inline void ul_dlist_init(struct ul_dlist *list)
{
    if(list) {
        list->next = list;
        list->prev = list;
    }
}

static inline void ul_dlist_append(struct ul_dlist *list, ul_dnode_t *node)
{
    if(list->next == list) {
        node->prev = list;
        node->next = list;
        list->next = node;
        list->prev = node;
    } else {
        node->next = list->next;
        node->prev = list;
        list->next = node;
    }
}


static inline void ul_dlist_prepend(struct ul_dlist *list, ul_dnode_t *node)
{
    node->next = list->next;
    node->prev = list;

    list->next->prev = node;
    list->next = node;
}

static inline void ul_dlist_insert_after(struct ul_dlist *list, ul_dnode_t *after, ul_dnode_t *node)
{
    if (!after) {
        ul_dlist_prepend(list, node);
    } else {
        node->next = after->next;
        node->prev = after;
        after->next->prev = node;
        after->next = node;
    }
}

static inline bool ul_dlist_is_empty(struct ul_dlist *list)
{
    return  (list->next == list);
}

static inline ul_dnode_t * ul_dlist_peek_head(struct ul_dlist *list)
{
    if(!ul_dlist_is_empty(list))
        return(list->prev);
    else
        return NULL;
}

static inline ul_dnode_t *ul_dlist_next(struct ul_dlist *list,
              ul_dnode_t *node)
{
  return (node == list->prev) ? NULL : node->next;
}

static inline void ul_dlist_remove_node(ul_dnode_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

#define ul_dlist_for_each_node(__list, __node_storage)          \
    for (__node_storage = ul_dlist_peek_head(__list); __node;   \
         __node = ul_dlist_next(__list, __node))
