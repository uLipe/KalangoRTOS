/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * IRQ handlers — kernel/irq/irq.c
 * Implements: syscall_router.h ul_kern_irq_* prototypes
 * Also implements: ul_kernel_irq_dispatch() called by arch vectors.S
 * Reference: docs/api_spec.md §10, docs/microkernel_book_tricore.md §10
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul/config.h>
#include <kernel/include/ul_irq_internal.h>
#include <kernel/include/ul_notif_internal.h>
#include <kernel/syscall/syscall_router.h>
#include <ul_arch.h>

void ul_irq_table_init(void)
{
	/* TODO: zero the binding table */
}

uint32_t ul_kern_irq_bind(uint32_t srpn, uint32_t notif, uint32_t bit)
{
	(void)srpn;
	(void)notif;
	(void)bit;
	/*
	 * TODO:
	 *   1. Look up notif object
	 *   2. Register binding: srpn → notif, bit
	 *   3. ul_arch_irq_src_configure(srpn, priority=srpn, cpu_id=0)
	 */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_irq_enable(uint32_t srpn)
{
	(void)srpn;
	/* TODO: ul_arch_irq_src_enable(srpn) */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_irq_disable(uint32_t srpn)
{
	(void)srpn;
	/* TODO: ul_arch_irq_src_disable(srpn) */
	return (uint32_t)(int32_t)UL_EINVAL;
}

uint32_t ul_kern_irq_ack(uint32_t srpn)
{
	(void)srpn;
	/* TODO: ul_arch_irq_src_ack(srpn) */
	return (uint32_t)(int32_t)UL_EINVAL;
}

/*
 * ul_kernel_irq_dispatch — called from vectors.S generic ISR handler.
 * Signals the notification bound to this SRPN (if any).
 */
void ul_kernel_irq_dispatch(uint8_t srpn)
{
	(void)srpn;
	/*
	 * TODO:
	 *   binding = ul_irq_binding_by_srpn(srpn)
	 *   if (binding) ul_kern_notif_signal(binding->notif->id, binding->bit)
	 */
}
