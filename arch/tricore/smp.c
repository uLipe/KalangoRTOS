/* SPDX-License-Identifier: MIT */
/*
 * TriCore SMP — hart id helpers live in arch.c; IPI + secondary bring-up here.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ulmk/config.h>
#include <ulmk_arch.h>
#include <arch_config.h>
#include <board_config.h>

#if defined(ULMK_BOARD_CPU1_PC)
#include <board_smp.h>
#endif

#if ULMK_CONFIG_ENABLE_SMP

#define SMP_GATE_WAIT	0x11111111u
#define SMP_GATE_READY	0xC0DEC0DEu

static volatile uint32_t g_smp_gate = SMP_GATE_WAIT;
static volatile uint32_t g_secondary_release[ULMK_ARCH_NUM_CPU];
static volatile uint32_t g_secondary_armed[ULMK_ARCH_NUM_CPU];
static void (*g_secondary_entry[ULMK_ARCH_NUM_CPU])(void);

/*
 * Soft mailbox: TC275 can latch ICR.PIPN from a remote GPSR SETR without
 * ever taking the BIV slot.  Idle drains this counter (see arch_cpu_idle).
 * Kernel-space IE=0 also masks delivery until return to a thread context.
 */
volatile uint32_t g_ulmk_ipi_soft[ULMK_ARCH_NUM_CPU];

extern char _trap_class0[];
extern char _ulmk_int_table[];
extern char _ulmk_isr_stack_cpu1_top[];
extern void _ulmk_cpu1_start(void);

static volatile uint32_t *ipi_src(uint32_t cpu)
{
	if (cpu == 0u)
		return (volatile uint32_t *)(uintptr_t)ULMK_BOARD_SRC_GPSR00;
#if defined(ULMK_BOARD_SRC_GPSR10)
	if (cpu == 1u)
		return (volatile uint32_t *)(uintptr_t)ULMK_BOARD_SRC_GPSR10;
#endif
#if defined(ULMK_BOARD_SRC_GPSR20)
	if (cpu == 2u)
		return (volatile uint32_t *)(uintptr_t)ULMK_BOARD_SRC_GPSR20;
#endif
	return NULL;
}

/*
 * Full SRC word for GPSR targeting @cpu.  Configuration bits only — callers
 * OR SETR / CLRR as needed.  Always rewrite the full config so a SETR write
 * cannot accidentally clear SRE/TOS (SRC write semantics).
 */
static uint32_t ipi_src_word(uint32_t cpu)
{
	return (uint32_t)ULMK_BOARD_IRQ_IPI |
	       ((uint32_t)cpu << ULMK_ARCH_SRC_TOS_SHIFT) |
	       (1u << ULMK_BOARD_SRC_SRE_BIT);
}

void ulmk_arch_smp_mark_ready(void)
{
	__asm__ volatile("dsync" ::: "memory");
	g_smp_gate = SMP_GATE_READY;
}

void ulmk_arch_send_ipi(uint32_t cpu_id)
{
	volatile uint32_t *src;

	if (cpu_id >= (uint32_t)ULMK_ARCH_NUM_CPU)
		return;
	if (cpu_id == ulmk_arch_cpu_id())
		return;
	src = ipi_src(cpu_id);
	if (!src)
		return;
	g_ulmk_ipi_soft[cpu_id]++;
	__asm__ volatile("dsync" ::: "memory");
	*src = ipi_src_word(cpu_id) | ULMK_ARCH_SRC_SETR_BIT;
}

void ulmk_arch_ipi_pulse_self(void)
{
	uint32_t           cpu = ulmk_arch_cpu_id();
	volatile uint32_t *src = ipi_src(cpu);

	if (!src)
		return;
	*src = ipi_src_word(cpu) | ULMK_ARCH_SRC_SETR_BIT;
}

void ulmk_arch_ipi_clear_self(void)
{
	uint32_t           cpu = ulmk_arch_cpu_id();
	volatile uint32_t *src = ipi_src(cpu);

	if (src)
		*src = ipi_src_word(cpu) | ULMK_ARCH_SRC_CLRR_BIT;
}

void ulmk_arch_ipi_note_enter(void)
{
	uint32_t cpu = ulmk_arch_cpu_id();

	if (cpu < (uint32_t)ULMK_ARCH_NUM_CPU)
		g_ulmk_ipi_soft[cpu] = 0u;
}

/*
 * Returns true once if a soft IPI is pending for this CPU (mailbox).
 * Used by idle when GPSR SETR fails to vector.
 */
bool ulmk_arch_ipi_soft_take(void)
{
	uint32_t cpu = ulmk_arch_cpu_id();
	uint32_t n;

	if (cpu >= (uint32_t)ULMK_ARCH_NUM_CPU)
		return false;
	n = g_ulmk_ipi_soft[cpu];
	if (n == 0u)
		return false;
	g_ulmk_ipi_soft[cpu] = 0u;
	__asm__ volatile("dsync" ::: "memory");
	return true;
}

/*
 * Two-step SRC bring-up per Infineon: program SRPN/TOS with SRE=0, CLRR,
 * then enable SRE.  Avoids sticky PIPN from a SETR+SRE race on first arm.
 */
static void ipi_src_arm(uint32_t cpu)
{
	volatile uint32_t *src = ipi_src(cpu);
	uint32_t           cfg;

	if (!src)
		return;
	cfg = (uint32_t)ULMK_BOARD_IRQ_IPI |
	      ((uint32_t)cpu << ULMK_ARCH_SRC_TOS_SHIFT);
	*src = cfg | ULMK_ARCH_SRC_CLRR_BIT;
	*src = cfg | (1u << ULMK_BOARD_SRC_SRE_BIT);
}

void ulmk_arch_secondary_init(void)
{
	uint32_t cpu = ulmk_arch_cpu_id();

	ulmk_arch_irq_vectors_init(
		(uintptr_t)_trap_class0,
		(uintptr_t)_ulmk_int_table,
		(uintptr_t)_ulmk_isr_stack_cpu1_top);
	ulmk_arch_mpu_init();
	ulmk_arch_mpu_enable();
	/*
	 * Arm receive GPSR before marking ready — CPU0 waits on
	 * g_secondary_armed so the first remote enqueue IPI is not lost.
	 * Two-step arm (CLRR then SRE) per Infineon SRC programming rules.
	 */
	ipi_src_arm(cpu);
}

void ulmk_arch_secondary_mark_ready(void)
{
	uint32_t cpu = ulmk_arch_cpu_id();

	__asm__ volatile("dsync" ::: "memory");
	g_secondary_armed[cpu] = 1u;
}

void ulmk_arch_start_secondary(uint32_t cpu_id, void (*entry)(void))
{
	if (cpu_id == 0u || cpu_id >= (uint32_t)ULMK_ARCH_NUM_CPU || !entry)
		return;

	ipi_src_arm(0u);
	ipi_src_arm(cpu_id);

	g_secondary_entry[cpu_id] = entry;
	g_secondary_armed[cpu_id] = 0u;
	__asm__ volatile("dsync" ::: "memory");
	g_secondary_release[cpu_id] = 1u;

#if defined(ULMK_BOARD_CPU1_PC)
	if (cpu_id == 1u)
		ulmk_board_cpu_start(1u, _ulmk_cpu1_start);
#else
	(void)cpu_id;
#endif

	/*
	 * Block until the secondary has BIV/ISP/MPU and its GPSR armed.
	 * Prevents CPU0 from enqueueing remote threads before IPIs can land.
	 */
	while (g_secondary_armed[cpu_id] == 0u)
		;
}

void ulmk_arch_smp_park(void)
{
	uint32_t hart = ulmk_arch_cpu_id();
	void (*entry)(void);

	if (hart == 0u || hart >= (uint32_t)ULMK_ARCH_NUM_CPU) {
		for (;;)
			;
	}

#if defined(ULMK_BOARD_CPU1_PC)
	/* Own WDT — cannot be unlocked safely from CPU0. */
	ulmk_board_cpu_wdt_disable_self();
#endif

	while (g_smp_gate != SMP_GATE_READY)
		;
	while (g_secondary_release[hart] == 0u)
		;

	entry = g_secondary_entry[hart];
	if (!entry) {
		for (;;)
			;
	}

	entry();
	for (;;)
		;
}

#else /* !ENABLE_SMP */

void ulmk_arch_smp_mark_ready(void) {}
void ulmk_arch_send_ipi(uint32_t cpu_id) { (void)cpu_id; }
void ulmk_arch_ipi_clear_self(void) {}
void ulmk_arch_ipi_note_enter(void) {}
void ulmk_arch_ipi_pulse_self(void) {}
bool ulmk_arch_ipi_soft_take(void) { return false; }
void ulmk_arch_secondary_init(void) {}
void ulmk_arch_secondary_mark_ready(void) {}
void ulmk_arch_start_secondary(uint32_t cpu_id, void (*entry)(void))
{
	(void)cpu_id;
	(void)entry;
}
void ulmk_arch_smp_park(void)
{
	for (;;)
		;
}

#endif /* ULMK_CONFIG_ENABLE_SMP */
