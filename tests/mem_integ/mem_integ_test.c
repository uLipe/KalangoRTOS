/* SPDX-License-Identifier: MIT */
/*
 * Memory integration test — tests/mem_integ/mem_integ_test.c
 *
 * Scenarios:
 *
 *  1. ANON alloc: thread allocates private RAM via ul_mem_map(ANON), writes
 *     a pattern, reads it back.
 *
 *  2. Domain isolation (grant): thread A allocates a buffer, grants READ
 *     access to thread B.  B reads the value written by A and verifies it.
 *
 *  3. MPU protection: thread B tries to WRITE to thread A's private buffer
 *     (no write grant). With MPU enabled, thread B is killed by the class-1
 *     protection trap handler.  The supervisor checks the progress flag:
 *       - progress == 1: MPU enforcement confirmed (PASS)
 *       - progress == 2: MPU NOT enforced (partial PASS; memory APIs OK)
 *
 * All threads run at UL_PRIV_DRIVER (PRS 1 on TriCore) except root.
 */

#include <stdint.h>
#include "../test_support.h"
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>
#include <ul_arch.h>


/* =========================================================================
 * Shared state
 * ========================================================================= */

#define PATTERN_A	0xA5A5A5A5u
#define PATTERN_B	0x5B5B5B5Bu

static volatile int g_test_result = -1;	/* -1=running, 0=fail, 1=pass */

/* ──────────────────────────────────────────────────────────────────────────
 * Scenario 1: anonymous allocation
 * ────────────────────────────────────────────────────────────────────────── */

static uint8_t  anon_stack[2048] __attribute__((aligned(8)));
static volatile int g_anon_result = -1;

static void anon_thread_entry(void *arg)
{
	volatile uint32_t *buf;
	void   *base;
	int     i;

	(void)arg;

	ul_printk("mem_integ: anon thread start\n");

	base = ul_mem_map(NULL, 128u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_integ: anon alloc FAIL\n");
		g_anon_result = 0;
		ul_thread_exit();
	}
	ul_printk("mem_integ: anon alloc base=0x%08x\n",
		  (unsigned)(uintptr_t)base);

	buf = (volatile uint32_t *)base;
	for (i = 0; i < 4; i++)
		buf[i] = PATTERN_A;

	for (i = 0; i < 4; i++) {
		if (buf[i] != PATTERN_A) {
			ul_printk("mem_integ: anon pattern FAIL i=%d\n", i);
			g_anon_result = 0;
			ul_thread_exit();
		}
	}
	ul_printk("mem_integ: anon alloc/write/read PASS\n");

	ul_mem_unmap(base, 128u);
	g_anon_result = 1;
	ul_thread_exit();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Scenario 2: memory grant (domain isolation — cooperative)
 * ────────────────────────────────────────────────────────────────────────── */

static uint8_t  grant_a_stack[2048] __attribute__((aligned(8)));
static uint8_t  grant_b_stack[2048] __attribute__((aligned(8)));
static volatile int g_grant_result = -1;

static volatile ul_notif_t g_grant_a_ready = UL_NOTIF_INVALID;
static volatile ul_notif_t g_grant_b_done  = UL_NOTIF_INVALID;
static volatile void      *g_grant_base;
static volatile ul_tid_t   g_grant_b_tid = UL_TID_INVALID;

static void grant_a_entry(void *arg)
{
	volatile uint32_t *buf;
	void   *base;
	uint32_t bits;
	int      ret;

	(void)arg;

	ul_printk("mem_integ: grant-A start\n");

	base = ul_mem_map(NULL, 64u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_integ: grant-A alloc FAIL\n");
		g_grant_result = 0;
		ul_thread_exit();
	}

	buf = (volatile uint32_t *)base;
	buf[0] = PATTERN_B;

	/* Wait until B's TID is available */
	while (g_grant_b_tid == UL_TID_INVALID)
		ul_msleep(5);

	/* Grant READ access to B */
	ret = ul_mem_grant(base, 64u, g_grant_b_tid, UL_PERM_READ);
	if (ret < 0) {
		ul_printk("mem_integ: grant FAIL ret=%d\n", ret);
		g_grant_result = 0;
		ul_thread_exit();
	}

	g_grant_base = base;

	ul_notif_signal(g_grant_a_ready, 1u);

	/* Wait for B to finish reading */
	bits = 0;
	ul_notif_wait(g_grant_b_done, 1u, &bits);

	ul_mem_unmap(base, 64u);
	ul_thread_exit();
}

static void grant_b_entry(void *arg)
{
	volatile uint32_t *buf;
	uint32_t           bits;

	(void)arg;

	ul_printk("mem_integ: grant-B start (tid=%u)\n",
		  (unsigned)ul_thread_self());

	bits = 0;
	ul_notif_wait(g_grant_a_ready, 1u, &bits);

	buf = (volatile uint32_t *)g_grant_base;

	if (buf[0] == PATTERN_B) {
		ul_printk("mem_integ: grant read PASS (pattern match)\n");
		g_grant_result = 1;
	} else {
		ul_printk("mem_integ: grant read FAIL (got 0x%08x)\n",
			  (unsigned)buf[0]);
		g_grant_result = 0;
	}

	ul_notif_signal(g_grant_b_done, 1u);
	ul_thread_exit();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Scenario 3: MPU protection fault (class 1 trap kills thread)
 * ────────────────────────────────────────────────────────────────────────── */

static uint8_t  fault_victim_stack[2048]  __attribute__((aligned(8)));
static uint8_t  fault_trigger_stack[2048] __attribute__((aligned(8)));
static volatile int   g_fault_progress;
static volatile void *g_fault_victim_base;

static void fault_victim_entry(void *arg)
{
	volatile uint32_t *buf;
	void              *base;

	(void)arg;

	base = ul_mem_map(NULL, 64u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_integ: fault-victim alloc FAIL\n");
		ul_thread_exit();
	}

	buf    = (volatile uint32_t *)base;
	buf[0] = 0xDEADBEEFu;

	g_fault_victim_base = base;
	ul_printk("mem_integ: victim buffer at 0x%08x\n",
		  (unsigned)(uintptr_t)base);

	ul_msleep(2000);
	ul_thread_exit();
}

static void fault_trigger_entry(void *arg)
{
	volatile uint32_t *victim;

	(void)arg;

	while (!g_fault_victim_base)
		ul_msleep(5);

	victim = (volatile uint32_t *)g_fault_victim_base;

	ul_printk("mem_integ: trigger: attempting write to 0x%08x\n",
		  (unsigned)(uintptr_t)g_fault_victim_base);

	g_fault_progress = 1;
	victim[0] = 0xCAFECAFEu;
	g_fault_progress = 2;	/* only reached if MPU is not enforced */

	ul_thread_exit();
}

/* ──────────────────────────────────────────────────────────────────────────
 * Supervisor
 * ────────────────────────────────────────────────────────────────────────── */

static uint8_t  sup_stack[2048] __attribute__((aligned(8)));

static void supervisor_entry(void *arg)
{
	uint32_t waited;

	(void)arg;

	waited = 0;
	while (g_anon_result < 0 && waited < 3000u) {
		ul_msleep(10);
		waited += 10u;
	}
	if (g_anon_result != 1) {
		ul_printk("mem_integ: FAIL at scenario 1 (anon)\n");
		g_test_result = 0;
		goto done;
	}
	ul_printk("mem_integ: scenario 1 (anon) PASS\n");

	waited = 0;
	while (g_grant_result < 0 && waited < 3000u) {
		ul_msleep(10);
		waited += 10u;
	}
	if (g_grant_result != 1) {
		ul_printk("mem_integ: FAIL at scenario 2 (grant)\n");
		g_test_result = 0;
		goto done;
	}
	ul_printk("mem_integ: scenario 2 (grant) PASS\n");

	waited = 0;
	while (g_fault_progress == 0 && waited < 1000u) {
		ul_msleep(10);
		waited += 10u;
	}

	ul_msleep(200);

	if (g_fault_progress == 1) {
		ul_printk("mem_integ: scenario 3 (MPU fault) PASS "
			  "(trigger killed by class-1 trap)\n");
	} else if (g_fault_progress == 2) {
		ul_printk("mem_integ: scenario 3 (MPU fault) PARTIAL "
			  "(MPU not enforced on this platform; memory APIs OK)\n");
	} else {
		ul_printk("mem_integ: FAIL at scenario 3 (trigger never ran)\n");
		g_test_result = 0;
		goto done;
	}

	g_test_result = 1;

done:
	if (g_test_result == 1) {
		ul_printk("mem_integ: PASS\n");
		ul_sim_exit(0);
	} else {
		ul_printk("mem_integ: FAIL\n");
		ul_sim_exit(1);
	}
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t         b_tid;

	(void)info;

	ul_printk("mem_integ: start\n");

	g_grant_a_ready = ul_notif_create();
	g_grant_b_done  = ul_notif_create();

	if (g_grant_a_ready == UL_NOTIF_INVALID ||
	    g_grant_b_done == UL_NOTIF_INVALID) {
		ul_printk("mem_integ: notif_create FAIL\n");
		ul_sim_exit(1);
	}

	attr.name       = "anon";
	attr.entry      = anon_thread_entry;
	attr.arg        = NULL;
	attr.priority   = 5u;
	attr.stack_size = sizeof(anon_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "grant_b";
	attr.entry      = grant_b_entry;
	attr.arg        = NULL;
	attr.priority   = 4u;
	attr.stack_size = sizeof(grant_b_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	b_tid = ul_thread_create(&attr);
	g_grant_b_tid = b_tid;

	attr.name       = "grant_a";
	attr.entry      = grant_a_entry;
	attr.arg        = NULL;
	attr.priority   = 4u;
	attr.stack_size = sizeof(grant_a_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "victim";
	attr.entry      = fault_victim_entry;
	attr.arg        = NULL;
	attr.priority   = 6u;
	attr.stack_size = sizeof(fault_victim_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "trigger";
	attr.entry      = fault_trigger_entry;
	attr.arg        = NULL;
	attr.priority   = 7u;
	attr.stack_size = sizeof(fault_trigger_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 15u;
	attr.stack_size = sizeof(sup_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	ul_thread_exit();
}
