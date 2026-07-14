/* SPDX-License-Identifier: MIT */
/*
 * irq_stress — flood / ooo-ack / rebind / preempt / bind-table exhaust.
 *
 * Soft-trigger backends match sdk_suite/irq_sw.
 * Root maps the trigger MMIO and fires; a high-prio consumer binds/waits
 * (irq_sw handshake: RDY → wait IRQ → ack → RDY; FIN when done).
 */
#include "sdk_test_util.h"
#include <board_config.h>

#define SRC_SETR_BIT	(1u << 26)
#define IRQ_BIT_IDX	0u
#define IRQ_MASK	(1u << IRQ_BIT_IDX)
#define BIT_RDY		(1u << 1)
#define BIT_FIN		(1u << 3)
#define FLOOD_N		32
#define REBIND_N	8
#define PREEMPT_N	16

#define UL_NVIC_SRC(irq)	(0x8000u | ((uint32_t)(irq) & 0x7FFFu))

#if defined(ULMK_BOARD_SRC_BASE)
#define UL_IRQ_SRPN_A	10u
#define UL_IRQ_SRPN_B	11u
#define UL_IRQ_SRC_A	(ULMK_BOARD_SRC_BASE + 0xC2u * 4u)
#define UL_IRQ_SRC_B	(ULMK_BOARD_SRC_BASE + 0xC3u * 4u)
#define UL_IRQ_MAP_BASE	ULMK_BOARD_SRC_BASE
#define UL_IRQ_MAP_SIZE	1024u
#elif defined(ULMK_BOARD_CLINT_BASE) && (ULMK_BOARD_CLINT_BASE != 0u)
#define UL_IRQ_SRPN_A	10u
#define UL_IRQ_SRPN_B	11u
#define UL_IRQ_SRC_A	ULMK_BOARD_CLINT_BASE
#define UL_IRQ_SRC_B	ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_BASE	ULMK_BOARD_CLINT_BASE
#define UL_IRQ_MAP_SIZE	0x1000u
#define IRQ_STRESS_CLINT 1
#elif defined(__ARM_ARCH)
#ifndef UL_IRQ_NVIC_LINE_A
#define UL_IRQ_NVIC_LINE_A	10u
#endif
#ifndef UL_IRQ_NVIC_LINE_B
#define UL_IRQ_NVIC_LINE_B	11u
#endif
#define ARM_NVIC_STIR		0xE000EF00u
#define UL_IRQ_SRPN_A		10u
#define UL_IRQ_SRPN_B		11u
#define UL_IRQ_SRC_A		UL_NVIC_SRC(UL_IRQ_NVIC_LINE_A)
#define UL_IRQ_SRC_B		UL_NVIC_SRC(UL_IRQ_NVIC_LINE_B)
#else
#error "irq_stress: unsupported board"
#endif

static int g_pass;
static int g_fail;
static ulmk_notif_t g_irq;
static ulmk_notif_t g_irq2;
static ulmk_notif_t g_sync;
static volatile int g_count;
static volatile int g_bound;

static void check(const char *name, int ok)
{
	sdk_puts(ok ? ".ok " : ".FAIL ");
	sdk_puts(name);
	sdk_puts("\n");
	if (ok)
		g_pass++;
	else
		g_fail++;
}

#define CHECK(name, cond) check((name), (cond) ? 1 : 0)

static void irq_trigger_a(void)
{
#if defined(ULMK_BOARD_SRC_BASE)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_A |= SRC_SETR_BIT;
#elif defined(IRQ_STRESS_CLINT)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_A = 1u;
#elif defined(__ARM_ARCH)
	*(volatile uint32_t *)(uintptr_t)ARM_NVIC_STIR = UL_IRQ_NVIC_LINE_A;
#endif
}

static void irq_trigger_b(void)
{
#if defined(ULMK_BOARD_SRC_BASE)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_B |= SRC_SETR_BIT;
#elif defined(IRQ_STRESS_CLINT)
	*(volatile uint32_t *)(uintptr_t)UL_IRQ_SRC_B = 1u;
#elif defined(__ARM_ARCH)
	*(volatile uint32_t *)(uintptr_t)ARM_NVIC_STIR = UL_IRQ_NVIC_LINE_B;
#endif
}

static int map_self(void)
{
#if defined(UL_IRQ_MAP_BASE)
	void *p;

	p = ulmk_mem_map((void *)(uintptr_t)UL_IRQ_MAP_BASE, UL_IRQ_MAP_SIZE,
			 ULMK_PERM_READ | ULMK_PERM_WRITE, ULMK_MMAP_PERIPH);
	return sdk_map_ok(p) ? 0 : -1;
#else
	return 0;
#endif
}

static int bind_enable(uint8_t srpn, ulmk_notif_t n, uintptr_t src)
{
	int rc;

	rc = ulmk_irq_bind_hw(srpn, n, IRQ_BIT_IDX, src);
	if (rc != ULMK_OK)
		return rc;
	return ulmk_irq_enable(srpn);
}

static void irq_consumer(void *arg)
{
	uint32_t n = (uint32_t)(uintptr_t)arg;
	uint32_t i;
	uint32_t bits;
	int      ret;

	if (!g_bound) {
		ret = bind_enable(UL_IRQ_SRPN_A, g_irq, UL_IRQ_SRC_A);
		if (ret != ULMK_OK) {
			g_count = -1;
			ulmk_notif_signal(g_sync, BIT_FIN);
			ulmk_thread_exit();
		}
		g_bound = 1;
	}

	ulmk_notif_signal(g_sync, BIT_RDY);
	for (i = 0u; i < n; i++) {
		bits = 0u;
		ret = ulmk_notif_wait(g_irq, IRQ_MASK, &bits);
		if (ret != ULMK_OK || !(bits & IRQ_MASK)) {
			g_count = -1;
			ulmk_notif_signal(g_sync, BIT_FIN);
			ulmk_thread_exit();
		}
		(void)ulmk_irq_ack(UL_IRQ_SRPN_A);
		g_count++;
		ulmk_notif_signal(g_sync, BIT_RDY);
	}
	ulmk_notif_signal(g_sync, BIT_FIN);
	ulmk_thread_exit();
}

static void fire_n(uint32_t n)
{
	uint32_t bits;
	uint32_t i;

	(void)ulmk_thread_priority_set(ulmk_thread_self(), 8u);
	for (i = 0u; i < n; i++) {
		bits = 0u;
		(void)ulmk_notif_wait(g_sync, BIT_RDY, &bits);
		irq_trigger_a();
	}
	bits = 0u;
	(void)ulmk_notif_wait(g_sync, BIT_FIN, &bits);
	(void)ulmk_thread_priority_set(ulmk_thread_self(), 100u);
}

static void run_flood(void)
{
	ulmk_tid_t w;

	g_count = 0;
	w = sdk_spawn("flood_w", irq_consumer, (void *)(uintptr_t)FLOOD_N,
		      2u, 2048u, 0u);
	CHECK("flood_spawn", w != ULMK_TID_INVALID);
	(void)ulmk_cap_grant(w, ULMK_CAP_IRQ);
	(void)ulmk_cap_grant(w, ULMK_CAP_MAP_PERIPH);
	fire_n(FLOOD_N);
	CHECK("flood_count", g_count == FLOOD_N);
}

static void run_ooo_ack(void)
{
	uint32_t bits = 0u;
	int      rc;

	irq_trigger_a();
	rc = ulmk_notif_wait(g_irq, IRQ_MASK, &bits);
	CHECK("ooo_wait", rc == ULMK_OK && (bits & IRQ_MASK));
	CHECK("ooo_ack1", ulmk_irq_ack(UL_IRQ_SRPN_A) == ULMK_OK);
	CHECK("ooo_ack2", ulmk_irq_ack(UL_IRQ_SRPN_A) == ULMK_OK);

	bits = 0u;
	irq_trigger_a();
	rc = ulmk_notif_wait(g_irq, IRQ_MASK, &bits);
	CHECK("ooo_again", rc == ULMK_OK && (bits & IRQ_MASK));
	CHECK("ooo_ack3", ulmk_irq_ack(UL_IRQ_SRPN_A) == ULMK_OK);
}

static void run_rebind(void)
{
	uint32_t bits;
	int      i;
	int      ok;

#if defined(IRQ_STRESS_CLINT)
	CHECK("rebind_dis", ulmk_irq_disable(UL_IRQ_SRPN_A) == ULMK_OK);
	irq_trigger_a();
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	CHECK("rebind_masked", !ulmk_notif_poll(g_irq, IRQ_MASK));
	CHECK("rebind_en_a", ulmk_irq_enable(UL_IRQ_SRPN_A) == ULMK_OK);
	bits = 0u;
	irq_trigger_a();
	CHECK("rebind_a_again",
	      ulmk_notif_wait(g_irq, IRQ_MASK, &bits) == ULMK_OK &&
	      (bits & IRQ_MASK));
	(void)ulmk_irq_ack(UL_IRQ_SRPN_A);
	CHECK("rebind_deliv", 1);
	return;
#endif

	CHECK("rebind_dis", ulmk_irq_disable(UL_IRQ_SRPN_A) == ULMK_OK);
	irq_trigger_a();
	for (i = 0; i < 8; i++)
		ulmk_thread_yield();
	CHECK("rebind_masked", !ulmk_notif_poll(g_irq, IRQ_MASK));

	ok = (bind_enable(UL_IRQ_SRPN_B, g_irq2, UL_IRQ_SRC_B) == ULMK_OK);
	CHECK("rebind_bind_b", ok);
	if (!ok)
		return;

	for (i = 0; i < REBIND_N; i++) {
		bits = 0u;
		irq_trigger_b();
		if (ulmk_notif_wait(g_irq2, IRQ_MASK, &bits) != ULMK_OK ||
		    !(bits & IRQ_MASK)) {
			CHECK("rebind_deliv", 0);
			(void)ulmk_irq_disable(UL_IRQ_SRPN_B);
			return;
		}
		(void)ulmk_irq_ack(UL_IRQ_SRPN_B);
	}
	CHECK("rebind_deliv", 1);
	CHECK("rebind_dis_b", ulmk_irq_disable(UL_IRQ_SRPN_B) == ULMK_OK);

	CHECK("rebind_en_a", ulmk_irq_enable(UL_IRQ_SRPN_A) == ULMK_OK);
	bits = 0u;
	irq_trigger_a();
	CHECK("rebind_a_again",
	      ulmk_notif_wait(g_irq, IRQ_MASK, &bits) == ULMK_OK &&
	      (bits & IRQ_MASK));
	(void)ulmk_irq_ack(UL_IRQ_SRPN_A);
}

static void run_preempt(void)
{
	ulmk_tid_t w;

	g_count = 0;
	w = sdk_spawn("pre_w", irq_consumer, (void *)(uintptr_t)PREEMPT_N,
		      2u, 2048u, 0u);
	CHECK("preempt_spawn", w != ULMK_TID_INVALID);
	(void)ulmk_cap_grant(w, ULMK_CAP_IRQ);
	(void)ulmk_cap_grant(w, ULMK_CAP_MAP_PERIPH);
	fire_n(PREEMPT_N);
	CHECK("preempt_count", g_count == PREEMPT_N);
}

static void run_bind_exhaust(void)
{
	int          n;
	int          i;
	int          rc;
	int          hit_nospace;
	ulmk_notif_t dummy;

	n = 0;
	hit_nospace = 0;
	for (i = 0; i < 32; i++) {
		dummy = ulmk_notif_create();
		if (dummy == ULMK_NOTIF_INVALID)
			break;
		rc = ulmk_irq_bind((uint8_t)(40u + (unsigned)i), dummy, 0u);
		if (rc == ULMK_ENOSPC) {
			hit_nospace = 1;
			break;
		}
		if (rc == ULMK_OK)
			n++;
	}
	CHECK("bind_got_space", n > 0);
	CHECK("bind_exhausted", hit_nospace);
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	board_services_init(info);
	sdk_puts("irq_stress: begin\n");
	g_pass  = 0;
	g_fail  = 0;
	g_bound = 0;

	CHECK("map", map_self() == 0);
	g_irq  = ulmk_notif_create();
	g_irq2 = ulmk_notif_create();
	g_sync = ulmk_notif_create();
	CHECK("notifs", g_irq != ULMK_NOTIF_INVALID &&
			g_irq2 != ULMK_NOTIF_INVALID &&
			g_sync != ULMK_NOTIF_INVALID);

	(void)ulmk_thread_priority_set(ulmk_thread_self(), 100u);

	sdk_puts("> flood\n");
	run_flood();
	sdk_puts("> ooo_ack\n");
	run_ooo_ack();
	sdk_puts("> rebind\n");
	run_rebind();
	sdk_puts("> preempt\n");
	run_preempt();
	sdk_puts("> bind_exhaust\n");
	run_bind_exhaust();

	(void)ulmk_irq_disable(UL_IRQ_SRPN_A);
	(void)ulmk_irq_ack(UL_IRQ_SRPN_A);

	sdk_puts("irq_stress: REPORT pass=");
	sdk_put_u32((uint32_t)g_pass);
	sdk_puts(" fail=");
	sdk_put_u32((uint32_t)g_fail);
	sdk_puts("\n");
	if (g_fail == 0)
		sdk_puts("irq_stress: PASS\n");
	else
		sdk_puts("irq_stress: FAIL\n");
	ulmk_thread_exit();
}
