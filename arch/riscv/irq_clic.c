/* SPDX-License-Identifier: MIT */
/*
 * CLIC interrupt backend — arch/riscv/irq_clic.c
 *
 * Core-local interrupts via MMIO (CLIC v0.9 layout).  Enabled when
 * ULMK_ARCH_HAVE_CLIC=1 in arch_config.h / board.cmake.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include <arch_config.h>
#include "irq_internal.h"

extern uint32_t g_src_addr[256];
extern uint8_t  g_src_type[256];

#if ULMK_ARCH_HAVE_CLIC

extern uint16_t g_src_clic_irq[256];

#define CLIC_INT_REGION_SIZE	0x1000u

static inline volatile uint8_t *clic_intip(uint32_t irq)
{
	return (volatile uint8_t *)(uintptr_t)
		(ULMK_ARCH_CLIC_INT_BASE + irq * 4u);
}

static inline volatile uint8_t *clic_intie(uint32_t irq)
{
	return clic_intip(irq) + 1u;
}

static inline bool clic_src_is_int_reg(uint32_t addr, uint32_t *irq_out)
{
	uint32_t off;

	if (addr < ULMK_ARCH_CLIC_INT_BASE)
		return false;
	off = addr - ULMK_ARCH_CLIC_INT_BASE;
	if (off >= CLIC_INT_REGION_SIZE)
		return false;
	*irq_out = off / 4u;
	return true;
}

void riscv_clic_init(void)
{
}

bool riscv_clic_is_binding(uint8_t srpn)
{
	return g_src_type[srpn] == IRQ_SRC_CLIC;
}

void riscv_clic_register(uint8_t srpn, uint32_t src_reg_addr)
{
	uint32_t irq;

	if (!clic_src_is_int_reg(src_reg_addr, &irq))
		return;

	g_src_type[srpn]     = IRQ_SRC_CLIC;
	g_src_clic_irq[srpn] = (uint16_t)irq;
	g_src_addr[srpn]     = src_reg_addr;
}

void riscv_clic_ack(uint8_t srpn)
{
	uint16_t irq;

	if (g_src_type[srpn] != IRQ_SRC_CLIC)
		return;
	irq = g_src_clic_irq[srpn];
	*clic_intip(irq) = 0u;
}

void riscv_clic_enable(uint8_t srpn)
{
	uint16_t irq;

	if (g_src_type[srpn] != IRQ_SRC_CLIC)
		return;
	irq = g_src_clic_irq[srpn];
	*clic_intie(irq) = 1u;
	__asm__ volatile("csrs mstatus, %0" :: "r"(MSTATUS_MIE_BIT));
}

void riscv_clic_disable(uint8_t srpn)
{
	uint16_t irq;

	if (g_src_type[srpn] != IRQ_SRC_CLIC)
		return;
	irq = g_src_clic_irq[srpn];
	*clic_intie(irq) = 0u;
}

void riscv_clic_dispatch(uint32_t mcause)
{
	uint32_t     irq;
	uint16_t     srpn;
	uint16_t     bound;

	irq = mcause & 0xFFFu;
	for (srpn = 1u; srpn < 256u; srpn++) {
		if (g_src_type[srpn] != IRQ_SRC_CLIC)
			continue;
		bound = g_src_clic_irq[srpn];
		if (bound != irq)
			continue;

		ulmk_kern_irq_dispatch((uint8_t)srpn);
		riscv_clic_ack((uint8_t)srpn);
	}
	_arch_generic_isr_handler();
}

#else /* !ULMK_ARCH_HAVE_CLIC */

void riscv_clic_init(void) { }
bool riscv_clic_is_binding(uint8_t srpn) { (void)srpn; return false; }
void riscv_clic_register(uint8_t srpn, uint32_t a) { (void)srpn; (void)a; }
void riscv_clic_enable(uint8_t srpn) { (void)srpn; }
void riscv_clic_disable(uint8_t srpn) { (void)srpn; }
void riscv_clic_ack(uint8_t srpn) { (void)srpn; }
void riscv_clic_dispatch(uint32_t mcause) { (void)mcause; }

#endif /* ULMK_ARCH_HAVE_CLIC */
