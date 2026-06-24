/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Internal IRQ binding table management.
 */

#ifndef UL_IRQ_INTERNAL_H
#define UL_IRQ_INTERNAL_H

#include <stdint.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_notif_internal.h>

typedef struct {
	uint8_t         srpn;
	ul_notif_obj_t *notif;
	uint32_t        bit;
	bool            enabled;
} ul_irq_binding_t;

void ul_irq_table_init(void);
int  ul_irq_binding_add(uint8_t srpn, ul_notif_obj_t *notif, uint32_t bit);
ul_irq_binding_t *ul_irq_binding_by_srpn(uint8_t srpn);

#endif /* UL_IRQ_INTERNAL_H */
