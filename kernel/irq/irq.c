/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IRQ binding table and kernel IRQ handlers — kernel/irq/irq.c
 * Reference: docs/api_spec.md §10
 *
 * Routing model:
 *   ulmk_irq_bind()   → records irq_nr→(notif_obj, bit) in irq_table
 *   ulmk_irq_enable() → arms the interrupt controller via arch layer
 *   ulmk_irq_ack()    → acknowledges the interrupt source (re-arming is driver's job)
 *   ulmk_kern_irq_dispatch() → called from generic ISR stub; signals the notif
 */

#include <stdint.h>
#include <string.h>
#include <ulmk/microkernel.h>
#include <ulmk/config.h>
#include <kernel/include/ulmk_irq_internal.h>
#include <kernel/include/ulmk_notif_internal.h>
#include <kernel/include/ulmk_mem_internal.h>
#include <kernel/syscall/syscall_router.h>
#include <ulmk_arch.h>

/* =========================================================================
 * Binding table — static, kernel-space, hardware-bounded size.
 * ========================================================================= */

static ulmk_irq_binding_t UL_KERNEL_BSS irq_table[ULMK_CONFIG_MAX_IRQ_BINDINGS];

void ulmk_irq_table_init(void)
{
	memset(irq_table, 0, sizeof(irq_table));
}

int ulmk_irq_binding_add(uint8_t srpn, ulmk_notif_obj_t *notif, uint32_t bit)
{
	int               i;
	ulmk_arch_irq_key_t key;

	key = ulmk_arch_cpu_irq_save();
	for (i = 0; i < ULMK_CONFIG_MAX_IRQ_BINDINGS; i++) {
		if (!irq_table[i].notif) {
			irq_table[i].srpn    = srpn;
			irq_table[i].notif   = notif;
			irq_table[i].bit     = bit;
			irq_table[i].enabled = false;
			ulmk_arch_cpu_irq_restore(key);
			return i;
		}
	}
	ulmk_arch_cpu_irq_restore(key);
	return -1;
}

ulmk_irq_binding_t *ulmk_irq_binding_by_srpn(uint8_t srpn)
{
	int i;

	for (i = 0; i < ULMK_CONFIG_MAX_IRQ_BINDINGS; i++) {
		if (irq_table[i].notif && irq_table[i].srpn == srpn)
			return &irq_table[i];
	}
	return NULL;
}

/* =========================================================================
 * Kernel syscall handlers
 * ========================================================================= */

uint32_t ulmk_kern_irq_bind(uint32_t srpn, uint32_t notif_id, uint32_t bit)
{
	ulmk_notif_obj_t *notif;
	int             ret;

	if (srpn == 0u || srpn >= 256u || bit > 31u)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	notif = ulmk_notif_by_id((ulmk_notif_t)notif_id);
	if (!notif)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	ret = ulmk_irq_binding_add((uint8_t)srpn, notif, bit);
	if (ret < 0)
		return (uint32_t)(int32_t)ULMK_ENOSPC;

	ulmk_arch_irq_src_configure((uint8_t)srpn, (uint8_t)srpn, 0u);
	return 0u;
}

uint32_t ulmk_kern_irq_bind_hw(uint32_t srpn, uint32_t notif_id,
			     uint32_t bit, uint32_t src_reg)
{
	ulmk_notif_obj_t *notif;
	int             ret;

	if (srpn == 0u || srpn >= 256u || bit > 31u || src_reg == 0u)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	notif = ulmk_notif_by_id((ulmk_notif_t)notif_id);
	if (!notif)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	ret = ulmk_irq_binding_add((uint8_t)srpn, notif, bit);
	if (ret < 0)
		return (uint32_t)(int32_t)ULMK_ENOSPC;

	ulmk_arch_irq_src_register((uint8_t)srpn, src_reg);
	return 0u;
}

uint32_t ulmk_kern_irq_enable(uint32_t srpn)
{
	ulmk_arch_irq_key_t  key;
	ulmk_irq_binding_t  *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	key = ulmk_arch_cpu_irq_save();
	b = ulmk_irq_binding_by_srpn((uint8_t)srpn);
	if (!b) {
		ulmk_arch_cpu_irq_restore(key);
		return (uint32_t)(int32_t)ULMK_EINVAL;
	}
	b->enabled = true;
	ulmk_arch_cpu_irq_restore(key);

	ulmk_arch_irq_src_enable((uint8_t)srpn);
	return 0u;
}

uint32_t ulmk_kern_irq_disable(uint32_t srpn)
{
	ulmk_arch_irq_key_t  key;
	ulmk_irq_binding_t  *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	key = ulmk_arch_cpu_irq_save();
	b = ulmk_irq_binding_by_srpn((uint8_t)srpn);
	if (!b) {
		ulmk_arch_cpu_irq_restore(key);
		return (uint32_t)(int32_t)ULMK_EINVAL;
	}
	b->enabled = false;
	ulmk_arch_cpu_irq_restore(key);

	ulmk_arch_irq_src_disable((uint8_t)srpn);
	ulmk_arch_irq_src_ack((uint8_t)srpn);
	return 0u;
}

uint32_t ulmk_kern_irq_ack(uint32_t srpn)
{
	ulmk_irq_binding_t *b;

	if (srpn == 0u || srpn >= 256u)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	b = ulmk_irq_binding_by_srpn((uint8_t)srpn);
	if (!b)
		return (uint32_t)(int32_t)ULMK_EINVAL;

	ulmk_arch_irq_src_ack((uint8_t)srpn);
	return 0u;
}

/* =========================================================================
 * ISR dispatch — called from the arch generic ISR handler.
 *
 * Runs in ISR context: global interrupts disabled, on the dedicated ISR stack.
 * Does not reschedule directly.  After this returns, the arch ISR stub calls
 * ulmk_kern_sched_dispatch(true), which may arm a deferred switch.
 * ========================================================================= */

void ulmk_kern_irq_dispatch(uint8_t srpn)
{
	ulmk_irq_binding_t *b = ulmk_irq_binding_by_srpn(srpn);

	if (!b || !b->enabled || !b->notif)
		return;

	notif_signal_impl(b->notif->id, 1u << b->bit);
}
