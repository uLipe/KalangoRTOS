/* SPDX-License-Identifier: MIT */
/*
 * IRQ integration test — tests/irq_integ/irq_integ_test.c
 *
 * Tests:
 *  1. ul_irq_bind/enable/wait/ack flow (3 iterations)
 *  2. ul_irq_disable: interrupt fires but notif is NOT signalled
 *  3. ul_irq_enable re-arms delivery after disable
 *
 * IRQ server (priority 2) and trigger (priority 8) run in parallel.
 * Software-trigger via ul_arch_irq_src_trigger (arch test helper).
 *
 * Test SRPN: UL_IRQ_TEST_SRPN (= 10), bit 0.
 */

#include <stdint.h>
#include "../test_support.h"
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>
#include <ul_arch.h>


/* =========================================================================
 * Shared state
 * ========================================================================= */

#define UL_IRQ_TEST_SRPN	10u
#define IRQ_BIT			(1u << 0)
#define ITER_COUNT		3

/* Notif created in root thread, shared with server and trigger. */
static volatile ul_notif_t g_irq_notif = UL_NOTIF_INVALID;

/*
 * "server ready" flag: trigger thread polls this before firing so
 * ul_irq_bind/enable always complete before the first trigger.
 * We use a second notification for synchronisation.
 */
static volatile ul_notif_t g_ready_notif = UL_NOTIF_INVALID;

static volatile int g_test_result = -1; /* -1 = running, 0 = fail, 1 = pass */

/* =========================================================================
 * IRQ server thread
 * ========================================================================= */

static uint8_t irq_srv_stack[2048] __attribute__((aligned(8)));

static void irq_server_entry(void *arg)
{
	uint32_t bits;
	int      i;
	int      ret;

	(void)arg;

	ul_printk("irq_integ: server start, binding SRPN=%u\n",
		  (unsigned)UL_IRQ_TEST_SRPN);

	ret = ul_irq_bind(UL_IRQ_TEST_SRPN, g_irq_notif, 0u);
	if (ret < 0) {
		ul_printk("irq_integ: bind FAIL ret=%d\n", ret);
		g_test_result = 0;
		ul_thread_exit();
	}

	ret = ul_irq_enable(UL_IRQ_TEST_SRPN);
	if (ret < 0) {
		ul_printk("irq_integ: enable FAIL ret=%d\n", ret);
		g_test_result = 0;
		ul_thread_exit();
	}

	/* Signal trigger that we're ready */
	ul_notif_signal(g_ready_notif, 1u);
	ul_printk("irq_integ: server ready, waiting for IRQs\n");

	/* --- Test 1: basic delivery loop ----------------------------------- */
	for (i = 0; i < ITER_COUNT; i++) {
		bits = 0;
		ret = ul_notif_wait(g_irq_notif, IRQ_BIT, &bits);
		if (ret < 0 || !(bits & IRQ_BIT)) {
			ul_printk("irq_integ: wait FAIL i=%d ret=%d bits=0x%x\n",
				  i, ret, (unsigned)bits);
			g_test_result = 0;
			ul_thread_exit();
		}
		ul_printk("irq_integ: IRQ received iter=%d\n", i);
		ul_irq_ack(UL_IRQ_TEST_SRPN);
	}
	ul_printk("irq_integ: basic IRQ delivery PASS\n");

	/* --- Test 2: disable prevents delivery ----------------------------- */
	ul_irq_disable(UL_IRQ_TEST_SRPN);

	/* Signal trigger to fire once while disabled */
	ul_notif_signal(g_ready_notif, (1u << 1));

	/* Sleep briefly; IRQ should NOT wake us */
	ul_msleep(50);

	bits = ul_notif_poll(g_irq_notif, IRQ_BIT);
	if (bits & IRQ_BIT) {
		ul_printk("irq_integ: disable test FAIL (IRQ delivered while disabled)\n");
		g_test_result = 0;
		ul_thread_exit();
	}
	ul_printk("irq_integ: IRQ disable PASS\n");

	/* --- Test 3: re-enable and receive --------------------------------- */
	ul_irq_enable(UL_IRQ_TEST_SRPN);

	/* Signal trigger to fire once more */
	ul_notif_signal(g_ready_notif, (1u << 2));

	bits = 0;
	ret = ul_notif_wait(g_irq_notif, IRQ_BIT, &bits);
	if (ret < 0 || !(bits & IRQ_BIT)) {
		ul_printk("irq_integ: re-enable wait FAIL ret=%d bits=0x%x\n",
			  ret, (unsigned)bits);
		g_test_result = 0;
		ul_thread_exit();
	}
	ul_irq_ack(UL_IRQ_TEST_SRPN);
	ul_printk("irq_integ: IRQ re-enable PASS\n");

	g_test_result = 1;
	ul_thread_exit();
}

/* =========================================================================
 * Trigger thread
 * ========================================================================= */

/*
 * TC27x SRC register block: SRC_BASE + slot * 4.
 * ul_arch_irq_src_configure() assigns slots sequentially from SRC_BASE.
 * The trigger thread needs peripheral write access to this entire block
 * so it can set the SETR bit for any SRPN via ul_arch_irq_src_trigger().
 */
#define TC27X_SRC_BASE	0xF0038000u
#define TC27X_SRC_SIZE	1024u		/* 256 slots × 4 bytes */

static uint8_t trigger_stack[1024] __attribute__((aligned(8)));

static void trigger_entry(void *arg)
{
	uint32_t bits;
	int      i;

	(void)arg;

	/*
	 * Map the TC27x SRC register block so ul_arch_irq_src_trigger() can
	 * write the SETR bit from driver context (PRS=1).
	 * Requires UL_CAP_MAP_PERIPH, which the root thread grants below.
	 */
	if (ul_mem_map((void *)TC27X_SRC_BASE, TC27X_SRC_SIZE,
		       UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH)
	    != (void *)TC27X_SRC_BASE) {
		ul_printk("irq_integ: trigger map SRC FAIL\n");
		g_test_result = 0;
		ul_thread_exit();
	}

	/* Wait for server to bind and enable */
	bits = 0;
	ul_notif_wait(g_ready_notif, 1u, &bits);
	ul_printk("irq_integ: trigger got ready signal\n");

	/* Test 1: fire ITER_COUNT times with gaps */
	for (i = 0; i < ITER_COUNT; i++) {
		ul_msleep(20);
		ul_printk("irq_integ: trigger firing IRQ iter=%d\n", i);
		ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);
		/* Yield so the server can handle before we fire again */
		ul_msleep(10);
	}

	/* Test 2: wait for server to disable, then fire (should be a no-op) */
	bits = 0;
	ul_notif_wait(g_ready_notif, (1u << 1), &bits);
	ul_printk("irq_integ: trigger firing while IRQ disabled\n");
	ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);
	ul_msleep(20); /* let server sleep pass */

	/* Test 3: wait for server to re-enable, then fire */
	bits = 0;
	ul_notif_wait(g_ready_notif, (1u << 2), &bits);
	ul_msleep(10);
	ul_printk("irq_integ: trigger firing after re-enable\n");
	ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);

	ul_thread_exit();
}

/* =========================================================================
 * Supervisor — waits for result
 * ========================================================================= */

static uint8_t sup_stack[1024] __attribute__((aligned(8)));

static void supervisor_entry(void *arg)
{
	(void)arg;

	/* Poll until test completes (max 5 seconds) */
	uint32_t waited = 0;
	while (g_test_result < 0 && waited < 5000u) {
		ul_msleep(50);
		waited += 50u;
	}

	if (g_test_result == 1) {
		ul_printk("irq_integ: PASS\n");
		ul_sim_exit(0);
	} else {
		ul_printk("irq_integ: FAIL (result=%d, waited=%u ms)\n",
			  g_test_result, (unsigned)waited);
		ul_sim_exit(1);
	}
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t         srv_tid;
	ul_tid_t         trig_tid;

	(void)info;

	ul_printk("irq_integ: start\n");

	g_irq_notif   = ul_notif_create();
	g_ready_notif = ul_notif_create();

	if (g_irq_notif == UL_NOTIF_INVALID ||
	    g_ready_notif == UL_NOTIF_INVALID) {
		ul_printk("irq_integ: notif_create FAIL\n");
		ul_sim_exit(1);
	}

	/* IRQ server — high priority, driver privilege (for irq_bind) */
	attr.name       = "irq_srv";
	attr.entry      = irq_server_entry;
	attr.arg        = NULL;
	attr.priority   = 2u;
	attr.stack_size = sizeof(irq_srv_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	srv_tid = ul_thread_create(&attr);

	/* Grant IRQ capability so the server can call ul_irq_bind/enable. */
	ul_cap_grant(srv_tid, UL_CAP_IRQ);

	/* Trigger — lower priority so server runs first */
	attr.name       = "trigger";
	attr.entry      = trigger_entry;
	attr.arg        = NULL;
	attr.priority   = 8u;
	attr.stack_size = sizeof(trigger_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	trig_tid = ul_thread_create(&attr);

	/* Grant peripheral-map capability so trigger can access SRC registers. */
	ul_cap_grant(trig_tid, UL_CAP_MAP_PERIPH);

	/* Supervisor — watches for completion */
	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 15u;
	attr.stack_size = sizeof(sup_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	ul_thread_exit();
}
