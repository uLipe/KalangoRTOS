/* SPDX-License-Identifier: MIT */
/*
 * CLINT interrupt backend — arch/riscv/irq_clint.c
 *
 * Machine timer (MTIP / mcause=0x80000007) and optional MSIP paths.
 * Enabled when ULMK_ARCH_HAVE_CLINT=1 in arch_config.h / board.cmake.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>
#include <arch_config.h>
#include "irq_internal.h"

extern uint32_t g_src_addr[256];

#if ULMK_ARCH_HAVE_CLINT

extern uint8_t g_src_type[256];

static inline void clint_mie_set(uint32_t addr, bool on)
{
	uint32_t bit = 0u;

	if (addr == ULMK_ARCH_CLINT_MSIP0)
		bit = 1u << 3;
	else if (addr == ULMK_ARCH_CLINT_MTIMECMP0)
		bit = 1u << 7;
	else
		return;

	if (on)
		__asm__ volatile("csrs mie, %0" :: "r"(bit));
	else
		__asm__ volatile("csrc mie, %0" :: "r"(bit));
}

void riscv_clint_init(void)
{
}

bool riscv_clint_is_binding(uint8_t srpn)
{
	return g_src_type[srpn] == IRQ_SRC_CLINT;
}

void riscv_clint_register(uint8_t srpn, uint32_t src_reg_addr)
{
	if (src_reg_addr != ULMK_ARCH_CLINT_MTIMECMP0 &&
	    src_reg_addr != ULMK_ARCH_CLINT_MSIP0)
		return;

	g_src_type[srpn] = IRQ_SRC_CLINT;
	g_src_addr[srpn] = src_reg_addr;
}

void riscv_clint_ack(uint8_t srpn)
{
	uint32_t addr;
	volatile uint32_t *cmp;

	addr = g_src_addr[srpn];
	if (!addr)
		return;

	if (addr == ULMK_ARCH_CLINT_MSIP0) {
		*(volatile uint32_t *)(uintptr_t)addr = 0u;
		return;
	}

	cmp = (volatile uint32_t *)(uintptr_t)addr;
	cmp[1] = 0xFFFFFFFFu;
	cmp[0] = 0xFFFFFFFFu;
}

void riscv_clint_enable(uint8_t srpn)
{
	uint32_t addr = g_src_addr[srpn];

	clint_mie_set(addr, false);
	riscv_clint_ack(srpn);
	clint_mie_set(addr, true);
}

void riscv_clint_disable(uint8_t srpn)
{
	clint_mie_set(g_src_addr[srpn], false);
}

void riscv_clint_dispatch(uint32_t mcause)
{
	uint16_t srpn;
	uint32_t addr;
	bool     is_soft;
	bool     is_timer;

	is_soft  = mcause == MCAUSE_MSOFT;
	is_timer = mcause == MCAUSE_MTIMER;

#if ULMK_CONFIG_ENABLE_SMP
	/*
	 * Reschedule IPI: clear this hart's MSIP; kernel marks needs_resched.
	 * Bound SRPN soft IRQs (tests) still run below.
	 */
	if (is_soft) {
		ulmk_arch_ipi_clear_self();
		ulmk_kern_ipi_resched();
	}
#endif

	for (srpn = 1u; srpn < 256u; srpn++) {
		if (g_src_type[srpn] != IRQ_SRC_CLINT)
			continue;

		addr = g_src_addr[srpn];
		if (is_soft && addr != ULMK_ARCH_CLINT_MSIP0)
			continue;
		if (is_timer && addr != ULMK_ARCH_CLINT_MTIMECMP0)
			continue;

		ulmk_kern_irq_dispatch((uint8_t)srpn);
		riscv_clint_ack((uint8_t)srpn);
	}
	_arch_generic_isr_handler();
}

#else /* !ULMK_ARCH_HAVE_CLINT */

void riscv_clint_init(void) { }
bool riscv_clint_is_binding(uint8_t srpn) { (void)srpn; return false; }
void riscv_clint_register(uint8_t srpn, uint32_t a) { (void)srpn; (void)a; }
void riscv_clint_enable(uint8_t srpn) { (void)srpn; }
void riscv_clint_disable(uint8_t srpn) { (void)srpn; }
void riscv_clint_ack(uint8_t srpn) { (void)srpn; }
void riscv_clint_dispatch(uint32_t mcause) { (void)mcause; }

#endif /* ULMK_ARCH_HAVE_CLINT */
