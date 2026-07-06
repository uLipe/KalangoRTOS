/* SPDX-License-Identifier: MIT */
/*
 * PLIC interrupt backend — arch/riscv/irq_plic.c
 *
 * Machine external interrupt (MEIP / mcause=0x8000000B).
 * Enabled when ULMK_ARCH_HAVE_PLIC=1 in arch_config.h / board.cmake.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include <arch_config.h>
#include "irq_internal.h"

#if ULMK_ARCH_HAVE_PLIC

extern uint32_t g_src_addr[256];
extern uint8_t  g_src_type[256];
extern uint32_t g_src_plic_id[256];
extern uint8_t  g_plic_to_srpn[64];
extern uint8_t  g_next_plic_id;

static inline volatile uint32_t *plic_claim_reg(void)
{
	return (volatile uint32_t *)(ULMK_ARCH_PLIC_CONTEXT_BASE + 4u);
}

static inline volatile uint32_t *plic_complete_reg(void)
{
	return (volatile uint32_t *)(ULMK_ARCH_PLIC_CONTEXT_BASE + 4u);
}

static inline void set_mie_meip(void)
{
	uint32_t bit = 1u << 11;

	__asm__ volatile("csrs mie, %0" :: "r"(bit));
}

void riscv_plic_init(void)
{
	volatile uint32_t *prio;
	uint32_t          i;

	prio = (volatile uint32_t *)ULMK_ARCH_PLIC_PRIORITY_BASE;
	for (i = 1u; i < 64u; i++)
		prio[i] = 1u;
}

void riscv_plic_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
	uint32_t id;

	(void)cpu_id;
	(void)priority;

	id = g_next_plic_id++;
	g_src_type[srpn] = IRQ_SRC_PLIC;
	g_src_plic_id[srpn] = id;
	g_plic_to_srpn[id] = srpn;
	g_src_addr[srpn] = ULMK_ARCH_PLIC_ENABLE_BASE +
			   (id / 32u) * ULMK_ARCH_PLIC_ENABLE_STRIDE;
}

void riscv_plic_register(uint8_t srpn, uint32_t src_reg_addr)
{
	uint32_t id;

	if (src_reg_addr == ULMK_ARCH_CLINT_MTIMECMP0 ||
	    src_reg_addr == ULMK_ARCH_CLINT_MSIP0)
		return;

	if (src_reg_addr < 0x00100000u) {
		id = (src_reg_addr >> 2) & 0x3Fu;
		if (id == 0u)
			id = g_next_plic_id++;
		g_src_type[srpn] = IRQ_SRC_PLIC;
		g_src_plic_id[srpn] = id;
		g_plic_to_srpn[id] = srpn;
		g_src_addr[srpn] = ULMK_ARCH_PLIC_ENABLE_BASE +
				   (id / 32u) * ULMK_ARCH_PLIC_ENABLE_STRIDE;
		return;
	}

	id = (src_reg_addr >> 2) & 0x3Fu;
	if (id == 0u)
		id = g_next_plic_id++;

	g_src_type[srpn] = IRQ_SRC_PLIC;
	g_src_plic_id[srpn] = id;
	g_plic_to_srpn[id] = srpn;
	g_src_addr[srpn] = src_reg_addr;
}

void riscv_plic_enable(uint8_t srpn)
{
	uint32_t addr;
	uint32_t bit;
	uint32_t id;

	addr = g_src_addr[srpn];
	id   = g_src_plic_id[srpn];
	if (!addr || !id)
		return;

	bit = 1u << (id & 31u);
	*(volatile uint32_t *)(uintptr_t)addr |= bit;
	set_mie_meip();
}

void riscv_plic_disable(uint8_t srpn)
{
	uint32_t addr;
	uint32_t bit;
	uint32_t id;

	addr = g_src_addr[srpn];
	id   = g_src_plic_id[srpn];
	if (!addr || !id)
		return;

	bit = 1u << (id & 31u);
	*(volatile uint32_t *)(uintptr_t)addr &= ~bit;
}

void riscv_plic_ack(uint8_t srpn)
{
	uint32_t id;

	id = g_src_plic_id[srpn];
	if (id)
		*plic_complete_reg() = id;
}

bool riscv_plic_is_pending(uint8_t srpn)
{
	(void)srpn;
	return false;
}

void riscv_plic_trigger(uint8_t srpn)
{
	uint32_t id;

	id = g_src_plic_id[srpn];
	if (id) {
		*(volatile uint32_t *)(ULMK_ARCH_PLIC_PENDING_BASE +
				       (id / 32u) * 4u) = 1u << (id & 31u);
	}
}

void riscv_plic_dispatch(void)
{
	uint32_t id;
	uint8_t  srpn;

	id = *plic_claim_reg();
	if (id == 0u || id >= 64u)
		return;

	srpn = g_plic_to_srpn[id];
	if (srpn == 0u)
		srpn = (uint8_t)id;

	ulmk_kern_irq_dispatch(srpn);
	*plic_complete_reg() = id;
	_arch_generic_isr_handler();
}

#else /* !ULMK_ARCH_HAVE_PLIC */

void riscv_plic_init(void) { }
void riscv_plic_configure(uint8_t s, uint8_t p, uint8_t c)
	{ (void)s; (void)p; (void)c; }
void riscv_plic_register(uint8_t s, uint32_t a) { (void)s; (void)a; }
void riscv_plic_enable(uint8_t s) { (void)s; }
void riscv_plic_disable(uint8_t s) { (void)s; }
void riscv_plic_ack(uint8_t s) { (void)s; }
bool riscv_plic_is_pending(uint8_t s) { (void)s; return false; }
void riscv_plic_trigger(uint8_t s) { (void)s; }
void riscv_plic_dispatch(void) { }

#endif /* ULMK_ARCH_HAVE_PLIC */
