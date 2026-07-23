/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IRQ binding table management.
 */

#ifndef UL_IRQ_INTERNAL_H
#define UL_IRQ_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_notif_internal.h>
#include <kernel/include/ulmk_thread_internal.h>

typedef struct {
	uint8_t			srpn;
	ulmk_notif_obj_t	*notif;
	uint32_t		bit;
	bool			enabled;
	/* Attach fast-path (NULL = bind-only legacy). */
	ulmk_irq_attach_fn_t	attach_fn;
	void			*attach_data;
	ulmk_thread_t		*owner;
	bool			owned_notif;	/* notif created by attach */
} ulmk_irq_binding_t;

void ulmk_irq_table_init(void);
int  ulmk_irq_binding_add(uint8_t srpn, ulmk_notif_obj_t *notif, uint32_t bit);
ulmk_irq_binding_t *ulmk_irq_binding_by_srpn(uint8_t srpn);

/*
 * Called from arch trap path when a class 0/1 fault hits while
 * ulmk_percpu()->in_irq_attach is set.  Kills the attach owner, tears
 * down the binding, and clears the per-CPU attach state.
 */
void ulmk_kern_irq_attach_fault(void);

/* True while a userspace attach callback is running on this CPU. */
bool ulmk_irq_in_attach(void);

#endif /* UL_IRQ_INTERNAL_H */
