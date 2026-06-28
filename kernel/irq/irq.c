/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IRQ binding table and kernel IRQ handlers — kernel/irq/irq.c
 * Reference: docs/api_spec.md §10, docs/microkernel_book_tricore.md §10
 *
 * Routing model:
 *   ul_irq_bind()   → records srpn→(notif_obj, bit) in irq_table
 *   ul_irq_enable() → arms the SRC register via arch layer
 *   ul_irq_ack()    → clears SRR in the SRC register (re-arming is driver's job)
 *   ul_kernel_irq_dispatch() → called from generic ISR stub; signals the notif
 */

#include <stdint.h>
#include <string.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_irq_internal.h>
#include <kernel/include/ul_notif_internal.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

/* =========================================================================
 * Binding table
 * ========================================================================= */

static ul_irq_binding_t irq_table[UL_CONFIG_MAX_IRQ_BINDINGS];

void ul_irq_table_init(void)
{
	memset(irq_table, 0, sizeof(irq_table));
}

int ul_irq_binding_add(uint8_t srpn, ul_notif_obj_t *notif, uint32_t bit)
{
	int               i;
	ul_arch_irq_key_t key;

	key = ul_arch_cpu_irq_save();
	for (i = 0; i < UL_CONFIG_MAX_IRQ_BINDINGS; i++) {
		if (!irq_table[i].notif) {
			irq_table[i].srpn    = srpn;
			irq_table[i].notif   = notif;
			irq_table[i].bit     = bit;
			irq_table[i].enabled = false;
			ul_arch_cpu_irq_restore(key);
			return i;
		}
	}
	ul_arch_cpu_irq_restore(key);
	return -1;
}

ul_irq_binding_t *ul_irq_binding_by_srpn(uint8_t srpn)
{
	int i;

	for (i = 0; i < UL_CONFIG_MAX_IRQ_BINDINGS; i++) {
		if (irq_table[i].notif && irq_table[i].srpn == srpn)
			return &irq_table[i];
	}
	return NULL;
}

/* =========================================================================
 * Kernel syscall handlers
 * ========================================================================= */

uint32_t ul_kern_irq_bind(uint32_t srpn, uint32_t notif_id, uint32_t bit)
{
	ul_notif_obj_t *notif;
	int             ret;

	if (srpn == 0u || srpn >= 256u || bit > 31u)
		return (uint32_t)(int32_t)UL_EINVAL;
	if (srpn == (uint32_t)UL_ARCH_TICK_SRPN)
		return (uint32_t)(int32_t)UL_EINVAL;

	notif = ul_notif_by_id((ul_notif_t)notif_id);
	if (!notif)
		return (uint32_t)(int32_t)UL_EINVAL;

	ret = ul_irq_binding_add((uint8_t)srpn, notif, bit);
	if (ret < 0)
		return (uint32_t)(int32_t)UL_ENOSPC;

	ul_arch_irq_src_configure((uint8_t)srpn, (uint8_t)srpn, 0u);
	return 0u;
}

uint32_t ul_kern_irq_enable(uint32_t srpn)
{
	ul_arch_irq_key_t  key;
	ul_irq_binding_t  *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)UL_EINVAL;

	key = ul_arch_cpu_irq_save();
	b = ul_irq_binding_by_srpn((uint8_t)srpn);
	if (!b) {
		ul_arch_cpu_irq_restore(key);
		return (uint32_t)(int32_t)UL_EINVAL;
	}
	b->enabled = true;
	ul_arch_cpu_irq_restore(key);

	ul_arch_irq_src_enable((uint8_t)srpn);
	return 0u;
}

uint32_t ul_kern_irq_disable(uint32_t srpn)
{
	ul_arch_irq_key_t  key;
	ul_irq_binding_t  *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)UL_EINVAL;

	key = ul_arch_cpu_irq_save();
	b = ul_irq_binding_by_srpn((uint8_t)srpn);
	if (!b) {
		ul_arch_cpu_irq_restore(key);
		return (uint32_t)(int32_t)UL_EINVAL;
	}
	b->enabled = false;
	ul_arch_cpu_irq_restore(key);

	ul_arch_irq_src_disable((uint8_t)srpn);
	return 0u;
}

uint32_t ul_kern_irq_ack(uint32_t srpn)
{
	ul_irq_binding_t *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)UL_EINVAL;

	b = ul_irq_binding_by_srpn((uint8_t)srpn);
	if (!b)
		return (uint32_t)(int32_t)UL_EINVAL;

	ul_arch_irq_src_ack((uint8_t)srpn);
	return 0u;
}

/* =========================================================================
 * ISR dispatch — called from _arch_generic_isr_handler in arch.c
 *
 * Runs in ISR context: ICR.IE=0, running on ISP.
 * Must NOT call ul_sched_schedule() — deferred scheduling happens via the
 * idle loop polling ul_sched_pick_next() after RFE.
 * ========================================================================= */

void ul_kernel_irq_dispatch(uint8_t srpn)
{
	ul_irq_binding_t *b = ul_irq_binding_by_srpn(srpn);

	if (!b || !b->enabled || !b->notif)
		return;

	notif_signal_impl(b->notif->id, 1u << b->bit);
}
