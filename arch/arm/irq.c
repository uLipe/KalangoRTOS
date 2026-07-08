/* SPDX-License-Identifier: MIT */
/*
 * NVIC IRQ glue — arch/arm/irq.c
 *
 * Maps kernel SRPNs onto Cortex-M NVIC external interrupt lines.  A board binds
 * an SRPN to a line via ulmk_irq_bind_hw(..., ULMK_ARCH_NVIC_SRC(line)); the
 * shared _arm_irq_entry stub (trap.S) recovers the line from IPSR and calls
 * _arm_irq_dispatch(line), which reverse-maps to the SRPN for the kernel.
 */

#include <stdint.h>
#include <stdbool.h>
#include <ulmk/microkernel.h>
#include <ulmk_arch.h>

#define REG32(a)	(*(volatile uint32_t *)(uintptr_t)(a))
#define REG8(a)		(*(volatile uint8_t  *)(uintptr_t)(a))

#define NVIC_EXC_PRIO	0xA0u

static uint16_t g_srpn_to_line[256];
static uint8_t  g_line_to_srpn[ULMK_ARCH_NUM_IRQ];
static bool     g_srpn_is_nvic[256];

static inline uint32_t line_word(uint32_t line)
{
	return line >> 5u;
}

static inline uint32_t line_bit(uint32_t line)
{
	return 1u << (line & 0x1Fu);
}

void ulmk_arch_irq_vectors_init(uintptr_t btv, uintptr_t biv, uintptr_t isp_top)
{
	(void)btv;
	(void)biv;
	(void)isp_top;
	/* VTOR is already set by startup.S; NVIC starts with all lines masked. */
}

void ulmk_arch_irq_src_configure(uint8_t srpn, uint8_t priority, uint8_t cpu_id)
{
	uint32_t line;

	(void)priority;
	(void)cpu_id;

	if (!g_srpn_is_nvic[srpn])
		return;
	line = g_srpn_to_line[srpn];
	if (line >= ULMK_ARCH_NUM_IRQ)
		return;

	/* Uniform priority: external IRQs never preempt SVC/SysTick or each other. */
	REG8(ULMK_ARCH_NVIC_IPR + line) = NVIC_EXC_PRIO;
}

void ulmk_arch_irq_src_register(uint8_t srpn, uint32_t src_reg_addr)
{
	uint32_t line;

	if (!(src_reg_addr & ULMK_ARCH_NVIC_SRC_TAG))
		return;

	line = src_reg_addr & 0x7FFFu;
	if (line >= ULMK_ARCH_NUM_IRQ)
		return;

	g_srpn_to_line[srpn] = (uint16_t)line;
	g_line_to_srpn[line] = srpn;
	g_srpn_is_nvic[srpn] = true;

	REG8(ULMK_ARCH_NVIC_IPR + line) = NVIC_EXC_PRIO;
}

void ulmk_arch_irq_src_enable(uint8_t srpn)
{
	uint32_t line;

	if (!g_srpn_is_nvic[srpn])
		return;
	line = g_srpn_to_line[srpn];
	REG32(ULMK_ARCH_NVIC_ISER + line_word(line) * 4u) = line_bit(line);
}

void ulmk_arch_irq_src_disable(uint8_t srpn)
{
	uint32_t line;

	if (!g_srpn_is_nvic[srpn])
		return;
	line = g_srpn_to_line[srpn];
	REG32(ULMK_ARCH_NVIC_ICER + line_word(line) * 4u) = line_bit(line);
}

void ulmk_arch_irq_src_ack(uint8_t srpn)
{
	uint32_t line;

	if (!g_srpn_is_nvic[srpn])
		return;
	line = g_srpn_to_line[srpn];
	REG32(ULMK_ARCH_NVIC_ICPR + line_word(line) * 4u) = line_bit(line);
	/*
	 * Re-arm delivery: _arm_irq_dispatch masked the line on entry so a still
	 * asserting level source could not storm the CPU before its driver ran.
	 * Ack means the driver has serviced and cleared the source, so the line
	 * is safe to re-enable — this mirrors the claim/complete re-arm on the
	 * RISC-V PLIC backend and keeps ulmk_irq_ack() sufficient for drivers.
	 */
	REG32(ULMK_ARCH_NVIC_ISER + line_word(line) * 4u) = line_bit(line);
}

bool ulmk_arch_irq_src_is_pending(uint8_t srpn)
{
	uint32_t line;

	if (!g_srpn_is_nvic[srpn])
		return false;
	line = g_srpn_to_line[srpn];
	return !!(REG32(ULMK_ARCH_NVIC_ISPR + line_word(line) * 4u) &
		  line_bit(line));
}

void ulmk_arch_irq_src_trigger(uint8_t srpn)
{
	uint32_t line;

	if (!g_srpn_is_nvic[srpn])
		return;
	line = g_srpn_to_line[srpn];
	REG32(ULMK_ARCH_NVIC_ISPR + line_word(line) * 4u) = line_bit(line);
}

/* Reverse map for the shared external-IRQ entry stub. */
uint8_t _arm_irq_line_to_srpn(uint32_t line)
{
	if (line >= ULMK_ARCH_NUM_IRQ)
		return 0u;
	return g_line_to_srpn[line];
}
