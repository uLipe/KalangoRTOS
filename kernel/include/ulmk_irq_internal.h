/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IRQ binding table management.
 */

#ifndef UL_IRQ_INTERNAL_H
#define UL_IRQ_INTERNAL_H

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_notif_internal.h>

typedef struct {
	uint8_t         srpn;
	ulmk_notif_obj_t *notif;
	uint32_t        bit;
	bool            enabled;
} ulmk_irq_binding_t;

void ulmk_irq_table_init(void);
int  ulmk_irq_binding_add(uint8_t srpn, ulmk_notif_obj_t *notif, uint32_t bit);
ulmk_irq_binding_t *ulmk_irq_binding_by_srpn(uint8_t srpn);

#endif /* UL_IRQ_INTERNAL_H */
