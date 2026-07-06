/* SPDX-License-Identifier: MIT */
/*
 * IRQ dispatch glue — arch/riscv/irq.c
 */

#include <stdint.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>
#include "irq_internal.h"

uint32_t g_src_addr[256];
uint8_t  g_src_type[256];
uint32_t g_src_plic_id[256];
uint8_t  g_plic_to_srpn[64];
uint8_t  g_next_plic_id = 2u;
uint16_t g_src_clic_irq[256];

#if ULMK_ARCH_HAVE_CLINT
extern void riscv_clint_init(void);
extern void riscv_clint_register(uint8_t srpn, uint32_t addr);
extern void riscv_clint_enable(uint8_t srpn);
extern void riscv_clint_disable(uint8_t srpn);
extern void riscv_clint_ack(uint8_t srpn);
extern void riscv_clint_dispatch(uint32_t mcause);
extern bool riscv_clint_is_binding(uint8_t srpn);
#else
static inline bool riscv_clint_is_binding(uint8_t srpn) { (void)srpn; return false; }
#endif

#if ULMK_ARCH_HAVE_CLIC
extern void riscv_clic_init(void);
extern void riscv_clic_register(uint8_t srpn, uint32_t addr);
extern void riscv_clic_enable(uint8_t srpn);
extern void riscv_clic_disable(uint8_t srpn);
extern void riscv_clic_ack(uint8_t srpn);
extern void riscv_clic_dispatch(uint32_t mcause);
extern bool riscv_clic_is_binding(uint8_t srpn);
#else
static inline bool riscv_clic_is_binding(uint8_t srpn) { (void)srpn; return false; }
#endif

#if ULMK_ARCH_HAVE_PLIC
extern void riscv_plic_init(void);
extern void riscv_plic_configure(uint8_t srpn, uint8_t p, uint8_t c);
extern void riscv_plic_register(uint8_t srpn, uint32_t addr);
extern void riscv_plic_enable(uint8_t srpn);
extern void riscv_plic_disable(uint8_t srpn);
extern void riscv_plic_ack(uint8_t srpn);
extern bool riscv_plic_is_pending(uint8_t srpn);
extern void riscv_plic_trigger(uint8_t srpn);
extern void riscv_plic_dispatch(void);
#endif

void _arch_generic_isr_handler(void)
{
	ulmk_kern_irq_check_preempt();
}

void riscv_irq_init(void)
{
#if ULMK_ARCH_HAVE_CLINT
	riscv_clint_init();
#endif
#if ULMK_ARCH_HAVE_CLIC
	riscv_clic_init();
#endif
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_init();
#endif
}

void riscv_irq_handle_interrupt(uint32_t mcause)
{
	if (!(mcause & MCAUSE_INT_BIT))
		return;

#if ULMK_ARCH_HAVE_PLIC
	if (mcause == MCAUSE_MEXT) {
		riscv_plic_dispatch();
		return;
	}
#endif
#if ULMK_ARCH_HAVE_CLINT
	if (mcause == MCAUSE_MSOFT || mcause == MCAUSE_MTIMER) {
		riscv_clint_dispatch(mcause);
		return;
	}
#endif
#if ULMK_ARCH_HAVE_CLIC
	riscv_clic_dispatch(mcause);
#endif
}

void ulmk_arch_irq_vectors_init(uintptr_t btv, uintptr_t biv, uintptr_t isp_top)
{
	(void)biv;
	(void)isp_top;

	__asm__ volatile("csrw mtvec, %0" :: "r"((uint32_t)btv));
	riscv_irq_init();
}

void ulmk_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_configure(srpn, priority, cpu_id);
#else
	(void)srpn;
	(void)priority;
	(void)cpu_id;
#endif
}

void ulmk_arch_irq_src_register(uint8_t srpn, uint32_t src_reg_addr)
{
#if ULMK_ARCH_HAVE_CLINT
	if (src_reg_addr == ULMK_ARCH_CLINT_MTIMECMP0 ||
	    src_reg_addr == ULMK_ARCH_CLINT_MSIP0) {
		riscv_clint_register(srpn, src_reg_addr);
		return;
	}
#endif
#if ULMK_ARCH_HAVE_CLIC
	if (src_reg_addr >= ULMK_ARCH_CLIC_INT_BASE &&
	    src_reg_addr < ULMK_ARCH_CLIC_INT_BASE + 0x1000u) {
		riscv_clic_register(srpn, src_reg_addr);
		return;
	}
#endif
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_register(srpn, src_reg_addr);
#else
	(void)srpn;
	(void)src_reg_addr;
#endif
}

void ulmk_arch_irq_src_enable(uint8_t srpn)
{
	if (riscv_clint_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLINT
		riscv_clint_enable(srpn);
#endif
		return;
	}
	if (riscv_clic_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLIC
		riscv_clic_enable(srpn);
#endif
		return;
	}
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_enable(srpn);
#endif
}

void ulmk_arch_irq_src_disable(uint8_t srpn)
{
	if (riscv_clint_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLINT
		riscv_clint_disable(srpn);
#endif
		return;
	}
	if (riscv_clic_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLIC
		riscv_clic_disable(srpn);
#endif
		return;
	}
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_disable(srpn);
#endif
}

void ulmk_arch_irq_src_ack(uint8_t srpn)
{
	if (riscv_clint_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLINT
		riscv_clint_ack(srpn);
#endif
		return;
	}
	if (riscv_clic_is_binding(srpn)) {
#if ULMK_ARCH_HAVE_CLIC
		riscv_clic_ack(srpn);
#endif
		return;
	}
#if ULMK_ARCH_HAVE_PLIC
	riscv_plic_ack(srpn);
#endif
}

bool ulmk_arch_irq_src_is_pending(uint8_t srpn)
{
#if ULMK_ARCH_HAVE_PLIC
	if (g_src_type[srpn] == IRQ_SRC_PLIC)
		return riscv_plic_is_pending(srpn);
#endif
	(void)srpn;
	return false;
}

void ulmk_arch_irq_src_trigger(uint8_t srpn)
{
#if ULMK_ARCH_HAVE_PLIC
	if (g_src_type[srpn] == IRQ_SRC_PLIC)
		riscv_plic_trigger(srpn);
#endif
	(void)srpn;
}
