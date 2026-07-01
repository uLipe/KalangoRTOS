/* SPDX-License-Identifier: MIT */
/*
 * IRQ integration test — tests/irq_integ/irq_integ_test.c
 *
 * Validates the full IRQ delivery path without relying on ul_msleep().
 * Flow is driven entirely by thread priorities and notifications:
 *
 *   Server  (prio 2) — binds IRQ, waits for notification.
 *   Trigger (prio 8) — runs when server blocks, fires IRQ via SRC SETR.
 *                      The generic ISR now checks preemption on exit, so
 *                      server immediately preempts trigger after each fire.
 *   Supervisor (prio 15) — only runs when both others are done/blocked;
 *                          verifies the iteration counter and exits.
 *
 * Three phases:
 *   1. Basic delivery: ITER_COUNT IRQ round-trips, no sleep.
 *   2. IRQ disable: trigger fires while server has disabled the IRQ;
 *                   server confirms notification is NOT signalled.
 *   3. IRQ re-enable: trigger fires after re-enable; server wakes normally.
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
#define BIT_SRV_RDY		(1u << 1)	/* server → trigger: step ready */
#define BIT_TRG_ACK		(1u << 2)	/* trigger → server: step done  */
#define ITER_COUNT		20

static volatile ul_notif_t g_irq_notif  = UL_NOTIF_INVALID;
static volatile ul_notif_t g_sync_notif = UL_NOTIF_INVALID;

static volatile int g_irq_count  = 0;	/* incremented by server per received IRQ */
static volatile int g_test_result = -1;	/* -1=running, 0=fail, 1=pass */

/* =========================================================================
 * IRQ server (priority 2 — highest)
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

	/* Signal trigger that we are ready; then block on first IRQ. */
	ul_notif_signal(g_sync_notif, BIT_SRV_RDY);
	ul_printk("irq_integ: server ready, waiting for IRQs\n");

	/* --- Phase 1: ITER_COUNT round-trips ----------------------------- */
	for (i = 0; i < ITER_COUNT; i++) {
		bits = 0;
		ret  = ul_notif_wait(g_irq_notif, IRQ_BIT, &bits);
		if (ret < 0 || !(bits & IRQ_BIT)) {
			ul_printk("irq_integ: wait FAIL i=%d\n", i);
			g_test_result = 0;
			ul_thread_exit();
		}
		ul_irq_ack(UL_IRQ_TEST_SRPN);
		g_irq_count++;
		/*
		 * Signal trigger to fire the next IRQ.  Because server (prio 2)
		 * is higher than trigger (prio 8), trigger only runs when server
		 * blocks on the next ul_notif_wait.
		 */
		ul_notif_signal(g_sync_notif, BIT_SRV_RDY);
	}
	ul_printk("irq_integ: basic IRQ delivery PASS (%d iters)\n", ITER_COUNT);

	/* --- Phase 2: IRQ disabled — trigger fires, server must NOT wake -- */
	ul_irq_disable(UL_IRQ_TEST_SRPN);

	/* Tell trigger to fire once while disabled; wait for its ack. */
	ul_notif_signal(g_sync_notif, BIT_SRV_RDY);
	bits = 0;
	ul_notif_wait(g_sync_notif, BIT_TRG_ACK, &bits);

	if (ul_notif_poll(g_irq_notif, IRQ_BIT)) {
		ul_printk("irq_integ: disable FAIL (IRQ delivered while disabled)\n");
		g_test_result = 0;
		ul_thread_exit();
	}
	ul_printk("irq_integ: IRQ disable PASS\n");

	/* --- Phase 3: re-enable, expect delivery -------------------------- */
	ul_irq_enable(UL_IRQ_TEST_SRPN);

	ul_notif_signal(g_sync_notif, BIT_SRV_RDY);
	bits = 0;
	ret  = ul_notif_wait(g_irq_notif, IRQ_BIT, &bits);
	if (ret < 0 || !(bits & IRQ_BIT)) {
		ul_printk("irq_integ: re-enable FAIL\n");
		g_test_result = 0;
		ul_thread_exit();
	}
	ul_irq_ack(UL_IRQ_TEST_SRPN);
	ul_printk("irq_integ: IRQ re-enable PASS\n");

	g_test_result = 1;
	ul_thread_exit();
}

/* =========================================================================
 * Trigger (priority 8 — lower than server)
 * ========================================================================= */

#define TC27X_SRC_BASE	0xF0038000u
#define TC27X_SRC_SIZE	1024u

static uint8_t trigger_stack[1024] __attribute__((aligned(8)));

static void trigger_entry(void *arg)
{
	uint32_t bits;
	int      i;

	(void)arg;

	if (ul_mem_map((void *)TC27X_SRC_BASE, TC27X_SRC_SIZE,
		       UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH)
	    != (void *)TC27X_SRC_BASE) {
		ul_printk("irq_integ: trigger map SRC FAIL\n");
		g_test_result = 0;
		ul_thread_exit();
	}

	/* Wait for server to bind and enable before firing. */
	bits = 0;
	ul_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);
	ul_printk("irq_integ: trigger got ready signal\n");

	/* --- Phase 1 ---------------------------------------------------- */
	for (i = 0; i < ITER_COUNT; i++) {
		ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);
		/*
		 * ISR fires → ul_kernel_irq_check_preempt() → server preempts
		 * trigger immediately on ISR exit.  Server processes, then
		 * signals BIT_SRV_RDY and blocks on next wait → trigger resumes.
		 */
		bits = 0;
		ul_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);
	}

	/* --- Phase 2: server disabled the IRQ, fire once and ack ---------- */
	ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);
	ul_notif_signal(g_sync_notif, BIT_TRG_ACK);

	/* Wait for server to re-enable before firing phase 3. */
	bits = 0;
	ul_notif_wait(g_sync_notif, BIT_SRV_RDY, &bits);

	/* --- Phase 3: fire after re-enable -------------------------------- */
	ul_arch_irq_src_trigger(UL_IRQ_TEST_SRPN);

	ul_thread_exit();
}

/* =========================================================================
 * Supervisor (priority 15 — lowest)
 * ========================================================================= */

static uint8_t sup_stack[1024] __attribute__((aligned(8)));

static void supervisor_entry(void *arg)
{
	(void)arg;

	/*
	 * Spin-wait until test_result is set.  Because supervisor has the
	 * lowest priority, it only gets CPU when both server and trigger
	 * have exited or are blocked.  A short yield loop is sufficient.
	 */
	while (g_test_result < 0)
		ul_thread_yield();

	if (g_test_result == 1 && g_irq_count == ITER_COUNT) {
		ul_printk("irq_integ: PASS\n");
		ul_sim_exit(0);
	} else {
		ul_printk("irq_integ: FAIL (result=%d count=%d)\n",
			  g_test_result, g_irq_count);
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

	g_irq_notif  = ul_notif_create();
	g_sync_notif = ul_notif_create();

	if (g_irq_notif == UL_NOTIF_INVALID || g_sync_notif == UL_NOTIF_INVALID) {
		ul_printk("irq_integ: notif_create FAIL\n");
		ul_sim_exit(1);
	}

	attr.name       = "irq_srv";
	attr.entry      = irq_server_entry;
	attr.arg        = NULL;
	attr.priority   = 2u;
	attr.stack_size = sizeof(irq_srv_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	srv_tid = ul_thread_create(&attr);
	ul_cap_grant(srv_tid, UL_CAP_IRQ);

	attr.name       = "trigger";
	attr.entry      = trigger_entry;
	attr.arg        = NULL;
	attr.priority   = 8u;
	attr.stack_size = sizeof(trigger_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	trig_tid = ul_thread_create(&attr);
	ul_cap_grant(trig_tid, UL_CAP_MAP_PERIPH);

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 15u;
	attr.stack_size = sizeof(sup_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	ul_thread_exit();
}
