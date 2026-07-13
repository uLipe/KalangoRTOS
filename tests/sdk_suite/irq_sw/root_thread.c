/* SPDX-License-Identifier: MIT */
/*
 * Software-triggered IRQ delivery via public irq_* API + board_config.
 *
 * TriCore: SRC SETR bit
 * RISC-V:  CLINT MSIP0
 * ARM:     NVIC STIR (src tagged like ULMK_ARCH_NVIC_SRC)
 */
#include "sdk_test_util.h"
#include <board_config.h>

#define UL_IRQ_SRPN		10u
#define IRQ_BIT			(1u << 0)
#define BIT_SRV_RDY		(1u << 1)
#define BIT_TRG_ACK		(1u << 2)
#define ITER_COUNT		10
#define SRC_SETR_BIT		(1u << 26)

/* Match arch/arm ULMK_ARCH_NVIC_SRC() — tag bit is part of the bind ABI. */
#define UL_NVIC_SRC(irq)	(0x8000u | ((uint32_t)(irq) & 0x7FFFu))

#if defined(ULMK_BOARD_SRC_BASE)
#define UL_IRQ_SRC_REG		(ULMK_BOARD_SRC_BASE + 0xC2u * 4u)
#define UL_IRQ_MAP_BASE		ULMK_BOARD_SRC_BASE
#define UL_IRQ_MAP_SIZE		1024u
#elif defined(ULMK_BOARD_CLINT_BASE) && (ULMK_BOARD_CLINT_BASE != 0u)
#define UL_IRQ_SRC_REG		ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_BASE		ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_SIZE		0x1000u
#elif defined(__ARM_ARCH)
#ifndef UL_IRQ_NVIC_LINE
#define UL_IRQ_NVIC_LINE	3u
#endif
#define ARM_NVIC_STIR		0xE000EF00u
#define UL_IRQ_SRC_REG		UL_NVIC_SRC(UL_IRQ_NVIC_LINE)
#else
#error "irq_sw: unsupported board"
#endif

static volatile ulmk_notif_t g_irq_notif  = ULMK_NOTIF_INVALID;
static volatile ulmk_notif_t g_sync_notif = ULMK_NOTIF_INVALID;
static volatile int          g_irq_count;
static volatile int          g_test_result = -1;

static void irq_sw_trigger(void)
{
#if defined(ULMK_BOARD_SRC_BASE)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_REG |= SRC_SETR_BIT;
#elif defined(ULMK_BOARD_CLINT_BASE) && (ULMK_BOARD_CLINT_BASE != 0u)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_REG = 1u;
#elif defined(__ARM_ARCH)
	*(volatile uint32_t *)(uintptr_t)ARM_NVIC_STIR = UL_IRQ_NVIC_LINE;
#endif
}

static void irq_server(void *arg)
{
	uint32_t bits;
	int      i;
	int      ret;

	(void)arg;

	ret = ulmk_irq_bind_hw(UL_IRQ_SRPN, g_irq_notif, 0u,
			       (uintptr_t)UL_IRQ_SRC_REG);
	if (ret != ULMK_OK) {
		sdk_puts("irq_sw: bind FAIL\n");
		g_test_result = 0;
		ulmk_thread_exit();
	}
	if (ulmk_irq_enable(UL_IRQ_SRPN) != ULMK_OK) {
		sdk_puts("irq_sw: enable FAIL\n");
		g_test_result = 0;
		ulmk_thread_exit();
	}

	ulmk_notif_signal(g_sync_notif, BIT_SRV_RDY);
	sdk_puts("irq_sw: server ready\n");

	for (i = 0; i < ITER_COUNT; i++) {
		bits = 0u;
		ret  = ulmk_notif_wait(g_irq_notif, IRQ_BIT, &bits);
		if (ret < 0 || !(bits & IRQ_BIT)) {
			g_test_result = 0;
			ulmk_thread_exit();
		}
		ulmk_irq_ack(UL_IRQ_SRPN);
		g_irq_count++;
		ulmk_notif_signal(g_sync_notif, BIT_SRV_RDY);
	}
	sdk_puts("irq_sw: basic delivery PASS\n");

	ulmk_irq_disable(UL_IRQ_SRPN);
	ulmk_notif_signal(g_sync_notif, BIT_SRV_RDY);
	bits = 0u;
	ulmk_notif_wait(g_sync_notif, BIT_TRG_ACK, &bits);
	if (ulmk_notif_poll(g_irq_notif, IRQ_BIT)) {
		sdk_puts("irq_sw: disable FAIL\n");
		g_test_result = 0;
		ulmk_thread_exit();
	}
	sdk_puts("irq_sw: disable PASS\n");

	ulmk_irq_enable(UL_IRQ_SRPN);
	ulmk_notif_signal(g_sync_notif, BIT_SRV_RDY);
	bits = 0u;
	ret  = ulmk_notif_wait(g_irq_notif, IRQ_BIT, &bits);
	if (ret < 0 || !(bits & IRQ_BIT)) {
		g_test_result = 0;
		ulmk_thread_exit();
	}
	ulmk_irq_ack(UL_IRQ_SRPN);
	sdk_puts("irq_sw: re-enable PASS\n");

	/*
	 * Disable before the final report.  On ARM, irq_ack re-arms ISER; leaving
	 * the line enabled while exiting can livelock the CPU in the IRQ path.
	 */
	ulmk_irq_disable(UL_IRQ_SRPN);
	ulmk_irq_ack(UL_IRQ_SRPN);

	if (g_irq_count == ITER_COUNT)
		sdk_puts("irq_sw: PASS\n");
	else
		sdk_puts("irq_sw: FAIL\n");
	ulmk_thread_exit();
}

static void trigger(void *arg)
{
	uint32_t bits;
	int      i;

	(void)arg;

#if defined(UL_IRQ_MAP_BASE)
	if (!sdk_map_ok(ulmk_mem_map((void *)(uintptr_t)UL_IRQ_MAP_BASE,
				     UL_IRQ_MAP_SIZE,
				     ULMK_PERM_READ | ULMK_PERM_WRITE,
				     ULMK_MMAP_PERIPH))) {
		g_test_result = 0;
		ulmk_thread_exit();
	}
#endif

	bits = 0u;
	ulmk_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);

	for (i = 0; i < ITER_COUNT; i++) {
		irq_sw_trigger();
		bits = 0u;
		ulmk_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);
	}

	irq_sw_trigger();
	ulmk_notif_signal(g_sync_notif, BIT_TRG_ACK);

	bits = 0u;
	ulmk_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);
	irq_sw_trigger();
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	sdk_puts("irq_sw: start\n");

	g_irq_notif  = ulmk_notif_create();
	g_sync_notif = ulmk_notif_create();

	tid = sdk_spawn("irq_srv", irq_server, NULL, 2u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_IRQ);
	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);

	tid = sdk_spawn("trigger", trigger, NULL, 8u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);
	(void)tid;
	ulmk_thread_exit();
}
