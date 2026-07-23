/* SPDX-License-Identifier: MIT */
/*
 * irq_attach — soft-triggered attach fast-path (all arches).
 *
 * Covers: true→notif+kernel ack, false→no notif+MMIO ack, data pointer,
 * syscall EPERM inside callback, detach, and optional isolation fault.
 */
#include "sdk_test_util.h"
#include <board_config.h>

#define UL_IRQ_SRPN		11u
#define IRQ_BIT			(1u << 0)
#define BIT_RDY			(1u << 1)
#define BIT_GO			(1u << 2)
#define SRC_SETR_BIT		(1u << 26)
#define SRC_CLRR_BIT		(1u << 25)

#define UL_NVIC_SRC(irq)	(0x8000u | ((uint32_t)(irq) & 0x7FFFu))

#if defined(ULMK_BOARD_SRC_BASE)
#define UL_IRQ_SRC_REG		(ULMK_BOARD_SRC_BASE + 0xC3u * 4u)
#define UL_IRQ_MAP_BASE		ULMK_BOARD_SRC_BASE
#define UL_IRQ_MAP_SIZE		1024u
#elif defined(ULMK_BOARD_CLINT_BASE) && (ULMK_BOARD_CLINT_BASE != 0u)
#define UL_IRQ_SRC_REG		ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_BASE		ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_SIZE		0x1000u
#elif defined(__ARM_ARCH)
#ifndef UL_IRQ_NVIC_LINE
#define UL_IRQ_NVIC_LINE	4u
#endif
#define ARM_NVIC_STIR		0xE000EF00u
#define ARM_NVIC_ICPR0		0xE000E280u
#define UL_IRQ_SRC_REG		UL_NVIC_SRC(UL_IRQ_NVIC_LINE)
#else
#error "irq_attach: unsupported board"
#endif

static volatile ulmk_notif_t g_sync = ULMK_NOTIF_INVALID;
static volatile int g_cb_hits;
static volatile int g_cb_data_ok;
static volatile int g_syscall_eperm;
static volatile int g_mode; /* 0=true, 1=false, 2=nosys, 3=isol */
static volatile uint32_t g_cookie = 0xA11ACu;
static volatile int g_overall = 1;

static void irq_hw_ack(void)
{
#if defined(ULMK_BOARD_SRC_BASE)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_REG |= SRC_CLRR_BIT;
#elif defined(ULMK_BOARD_CLINT_BASE) && (ULMK_BOARD_CLINT_BASE != 0u)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_REG = 0u;
#elif defined(__ARM_ARCH)
	*(volatile uint32_t *)(uintptr_t)ARM_NVIC_ICPR0 =
		(1u << (UL_IRQ_NVIC_LINE & 31u));
#endif
}

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

static bool attach_cb(void *data)
{
	g_cb_hits++;
	if (data && *(volatile uint32_t *)data == 0xA11ACu)
		g_cb_data_ok = 1;

	if (g_mode == 2) {
		int rc = ulmk_notif_signal(g_sync, BIT_GO);

		g_syscall_eperm = (rc == ULMK_EPERM) ? 1 : 0;
		irq_hw_ack();
		return false;
	}

	if (g_mode == 3) {
		typedef void (*fn_t)(void);
		fn_t fn = (fn_t)(uintptr_t)0x1000u;

		fn();
		return false;
	}

	if (g_mode == 1) {
		irq_hw_ack();
		return false;
	}

	return true;
}

static void attach_server(void *arg)
{
	ulmk_notif_t n;
	uint32_t bits;
	int ret;

	(void)arg;

#if defined(UL_IRQ_MAP_BASE)
	if (!sdk_map_ok(ulmk_mem_map((void *)(uintptr_t)UL_IRQ_MAP_BASE,
				     UL_IRQ_MAP_SIZE,
				     ULMK_PERM_READ | ULMK_PERM_WRITE,
				     ULMK_MMAP_PERIPH))) {
		sdk_puts("irq_attach: map FAIL\n");
		g_overall = 0;
		ulmk_notif_signal(g_sync, BIT_RDY);
		ulmk_thread_exit();
	}
#endif

	/* ── true path: kernel ack + notif ── */
	g_mode = 0;
	g_cb_hits = 0;
	g_cb_data_ok = 0;
	n = ulmk_irq_attach_hw(UL_IRQ_SRPN, attach_cb, (void *)&g_cookie,
			       (uintptr_t)UL_IRQ_SRC_REG);
	/* Errors are small negative ULMK_E*; notif handles may be high ptrs. */
	if (n == ULMK_NOTIF_INVALID ||
	    ((int32_t)n < 0 && (int32_t)n >= -16)) {
		sdk_puts("irq_attach: attach FAIL\n");
		g_overall = 0;
		ulmk_notif_signal(g_sync, BIT_RDY);
		ulmk_thread_exit();
	}
	if (ulmk_irq_enable(UL_IRQ_SRPN) != ULMK_OK) {
		sdk_puts("irq_attach: enable FAIL\n");
		g_overall = 0;
		ulmk_thread_exit();
	}

	ulmk_notif_signal(g_sync, BIT_RDY);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	irq_sw_trigger();

	bits = 0u;
	ret = ulmk_notif_wait(n, IRQ_BIT, &bits);
	if (ret < 0 || !(bits & IRQ_BIT) || g_cb_hits != 1 || !g_cb_data_ok) {
		sdk_puts("irq_attach: true path FAIL\n");
		g_overall = 0;
	} else {
		sdk_puts("irq_attach: true path PASS\n");
	}

	ulmk_irq_disable(UL_IRQ_SRPN);
	if (ulmk_irq_detach(UL_IRQ_SRPN) != ULMK_OK) {
		sdk_puts("irq_attach: detach FAIL\n");
		g_overall = 0;
	} else {
		sdk_puts("irq_attach: detach PASS\n");
	}

	/* ── false path: no notif, callback acks ── */
	g_mode = 1;
	g_cb_hits = 0;
	n = ulmk_irq_attach_hw(UL_IRQ_SRPN, attach_cb, (void *)&g_cookie,
			       (uintptr_t)UL_IRQ_SRC_REG);
	if (((int32_t)n < 0 && (int32_t)n >= -16) ||
	    n == ULMK_NOTIF_INVALID ||
	    ulmk_irq_enable(UL_IRQ_SRPN) != ULMK_OK) {
		sdk_puts("irq_attach: false reattach FAIL\n");
		g_overall = 0;
		ulmk_thread_exit();
	}

	ulmk_notif_signal(g_sync, BIT_RDY);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	irq_sw_trigger();

	/* Give ISR time; poll must stay clear. */
	{
		int i;

		for (i = 0; i < 50; i++)
			ulmk_thread_yield();
	}
	if (ulmk_notif_poll(n, IRQ_BIT) != 0u || g_cb_hits != 1) {
		sdk_puts("irq_attach: false path FAIL\n");
		g_overall = 0;
	} else {
		sdk_puts("irq_attach: false path PASS\n");
	}

	ulmk_irq_disable(UL_IRQ_SRPN);
	(void)ulmk_irq_detach(UL_IRQ_SRPN);

	/*
	 * Nested syscall from ISR is reliable on TriCore (class-6 trap).
	 * RISC-V/ARM nested ecall/SVC from the IRQ path is not used — the
	 * router still gates on in_irq_attach (covered by unit tests).
	 */
#if defined(__TRICORE__) || defined(__tricore__)
	g_mode = 2;
	g_cb_hits = 0;
	g_syscall_eperm = 0;
	n = ulmk_irq_attach_hw(UL_IRQ_SRPN, attach_cb, (void *)&g_cookie,
			       (uintptr_t)UL_IRQ_SRC_REG);
	if (((int32_t)n < 0 && (int32_t)n >= -16) ||
	    n == ULMK_NOTIF_INVALID ||
	    ulmk_irq_enable(UL_IRQ_SRPN) != ULMK_OK) {
		sdk_puts("irq_attach: nosys reattach FAIL\n");
		g_overall = 0;
		ulmk_thread_exit();
	}

	ulmk_notif_signal(g_sync, BIT_RDY);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	irq_sw_trigger();
	{
		int i;

		for (i = 0; i < 50; i++)
			ulmk_thread_yield();
	}
	if (!g_syscall_eperm || g_cb_hits != 1) {
		sdk_puts("irq_attach: no_syscall FAIL\n");
		g_overall = 0;
	} else {
		sdk_puts("irq_attach: no_syscall PASS\n");
	}

	ulmk_irq_disable(UL_IRQ_SRPN);
	(void)ulmk_irq_detach(UL_IRQ_SRPN);
#else
	sdk_puts("irq_attach: no_syscall SKIP\n");
	/* consume the third trigger handshake */
	ulmk_notif_signal(g_sync, BIT_RDY);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
#endif

	if (g_overall)
		sdk_puts("irq_attach: PASS\n");
	else
		sdk_puts("irq_attach: FAIL\n");
	ulmk_thread_exit();
}

static void trigger(void *arg)
{
	uint32_t bits;
	int i;

	(void)arg;

#if defined(UL_IRQ_MAP_BASE)
	if (!sdk_map_ok(ulmk_mem_map((void *)(uintptr_t)UL_IRQ_MAP_BASE,
				     UL_IRQ_MAP_SIZE,
				     ULMK_PERM_READ | ULMK_PERM_WRITE,
				     ULMK_MMAP_PERIPH))) {
		ulmk_thread_exit();
	}
#endif

	for (i = 0; i < 3; i++) {
		bits = 0u;
		ulmk_notif_wait(g_sync, BIT_RDY, &bits);
		ulmk_notif_signal(g_sync, BIT_GO);
	}
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_tid_t tid;

	board_services_init(info);
	sdk_puts("irq_attach: start\n");

	g_sync = ulmk_notif_create();
	tid = sdk_spawn("att_srv", attach_server, NULL, 2u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_IRQ);
	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);

	tid = sdk_spawn("trigger", trigger, NULL, 8u, 2048u, 0u);
	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);
	(void)tid;
	ulmk_thread_exit();
}
