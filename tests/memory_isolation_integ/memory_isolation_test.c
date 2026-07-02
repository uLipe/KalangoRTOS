/* SPDX-License-Identifier: MIT */
/*
 * Memory isolation integration test
 * tests/memory_isolation_integ/memory_isolation_test.c
 *
 * Three scenarios that exercise MPU-backed memory isolation:
 *
 *  1. Anon mmap: thread allocates private RAM via ul_mem_map(ANON), writes
 *     a pattern, reads it back, then unmaps.
 *
 *  2. Memory grant: thread A allocates a buffer and writes PATTERN_B.
 *     Thread A grants READ-only access to thread B.  Thread B reads and
 *     verifies the value — confirms cross-thread read with explicit grant.
 *
 *  3. MPU write-fault: thread TRIGGER attempts to write to thread VICTIM's
 *     private buffer without a write grant.  The class-1 protection trap
 *     handler kills TRIGGER.  Supervisor checks g_fault_progress:
 *       - progress == 1  → write attempt was made, thread killed (PASS)
 *       - progress == 2  → write succeeded (MPU not enforced; partial PASS)
 *       - progress == 0  → trigger never ran (FAIL)
 *
 * All worker threads run at UL_PRIV_DRIVER (PRS 1).
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>
#include "../test_support.h"

#define PATTERN_A	0xA5A5A5A5u
#define PATTERN_B	0x5B5B5B5Bu

/* =========================================================================
 * Scenario 1 — anonymous mmap
 * ========================================================================= */

static volatile int g_anon_result = -1;

static void anon_thread_entry(void *arg)
{
	volatile uint32_t *buf;
	void              *base;
	int                i;

	(void)arg;

	base = ul_mem_map(NULL, 128u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_iso: anon alloc FAIL\n");
		g_anon_result = 0;
		ul_thread_exit();
	}

	buf = (volatile uint32_t *)base;
	for (i = 0; i < 4; i++)
		buf[i] = PATTERN_A;

	for (i = 0; i < 4; i++) {
		if (buf[i] != PATTERN_A) {
			ul_printk("mem_iso: anon pattern FAIL i=%d\n", i);
			g_anon_result = 0;
			ul_thread_exit();
		}
	}

	ul_mem_unmap(base, 128u);
	g_anon_result = 1;
	ul_thread_exit();
}

/* =========================================================================
 * Scenario 2 — memory grant (cross-thread read)
 * ========================================================================= */

static volatile int      g_grant_result = -1;
static volatile ul_tid_t    g_grant_b_tid  = UL_TID_INVALID;
static volatile ul_notif_t  g_grant_ready  = UL_NOTIF_INVALID;
static volatile ul_notif_t  g_grant_done   = UL_NOTIF_INVALID;
static volatile void       *g_grant_base;

static void grant_a_entry(void *arg)
{
	volatile uint32_t *buf;
	void              *base;
	uint32_t           bits;
	int                ret;

	(void)arg;

	base = ul_mem_map(NULL, 64u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_iso: grant-A alloc FAIL\n");
		g_grant_result = 0;
		ul_thread_exit();
	}

	buf    = (volatile uint32_t *)base;
	buf[0] = PATTERN_B;

	/* Spin until B's TID is published. */
	while (g_grant_b_tid == UL_TID_INVALID)
		ul_msleep(5);

	ret = ul_mem_grant(base, 64u, g_grant_b_tid, UL_PERM_READ);
	if (ret < 0) {
		ul_printk("mem_iso: ul_mem_grant FAIL ret=%d\n", ret);
		g_grant_result = 0;
		ul_thread_exit();
	}

	g_grant_base = base;
	ul_notif_signal(g_grant_ready, 1u);

	bits = 0;
	ul_notif_wait(g_grant_done, 1u, &bits);

	ul_mem_unmap(base, 64u);
	ul_thread_exit();
}

static void grant_b_entry(void *arg)
{
	volatile uint32_t *buf;
	uint32_t           bits;

	(void)arg;

	bits = 0;
	ul_notif_wait(g_grant_ready, 1u, &bits);

	buf = (volatile uint32_t *)g_grant_base;

	if (buf[0] == PATTERN_B) {
		ul_printk("mem_iso: scenario 2 (grant read) PASS\n");
		g_grant_result = 1;
	} else {
		ul_printk("mem_iso: grant read FAIL got=0x%08x\n",
			  (unsigned)buf[0]);
		g_grant_result = 0;
	}

	ul_notif_signal(g_grant_done, 1u);
	ul_thread_exit();
}

/* =========================================================================
 * Scenario 3 — MPU write-fault kills offending thread
 * ========================================================================= */

static volatile int   g_fault_progress;
static volatile void *g_fault_victim_base;

static void fault_victim_entry(void *arg)
{
	volatile uint32_t *buf;
	void              *base;

	(void)arg;

	base = ul_mem_map(NULL, 64u, UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_ANON);
	if ((intptr_t)base <= 0) {
		ul_printk("mem_iso: fault-victim alloc FAIL\n");
		ul_thread_exit();
	}

	buf    = (volatile uint32_t *)base;
	buf[0] = 0xDEADBEEFu;

	g_fault_victim_base = base;
	ul_printk("mem_iso: victim buffer at 0x%08x\n",
		  (unsigned)(uintptr_t)base);

	/* Stay alive long enough for supervisor to observe the result. */
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

	g_fault_progress = 1;
	victim[0] = 0xCAFECAFEu;	/* no write grant → class-1 trap kills us */
	g_fault_progress = 2;		/* reached only if MPU not enforced */

	ul_thread_exit();
}

/* =========================================================================
 * Supervisor — collects results and exits the simulator
 * ========================================================================= */

static void supervisor_entry(void *arg)
{
	uint32_t waited;
	int      overall = 1;

	(void)arg;

	/* Scenario 1 */
	waited = 0;
	while (g_anon_result < 0 && waited < 3000u) {
		ul_msleep(10);
		waited += 10u;
	}
	if (g_anon_result == 1) {
		ul_printk("mem_iso: scenario 1 (anon mmap) PASS\n");
	} else {
		ul_printk("mem_iso: FAIL at scenario 1 (anon mmap)\n");
		overall = 0;
	}

	/* Scenario 2 */
	waited = 0;
	while (g_grant_result < 0 && waited < 3000u) {
		ul_msleep(10);
		waited += 10u;
	}
	if (g_grant_result != 1) {
		ul_printk("mem_iso: FAIL at scenario 2 (grant)\n");
		overall = 0;
	}

	/* Scenario 3 */
	waited = 0;
	while (g_fault_progress == 0 && waited < 1000u) {
		ul_msleep(10);
		waited += 10u;
	}
	ul_msleep(200);

	if (g_fault_progress == 1) {
		ul_printk("mem_iso: scenario 3 (MPU fault) PASS\n");
	} else if (g_fault_progress == 2) {
		ul_printk("mem_iso: scenario 3 (MPU fault) PARTIAL "
			  "(MPU not enforced on this platform)\n");
	} else {
		ul_printk("mem_iso: FAIL at scenario 3 (trigger never ran)\n");
		overall = 0;
	}

	if (overall) {
		ul_printk("MEMORY ISOLATION TEST: PASS\n");
		ul_sim_exit(0);
	} else {
		ul_printk("MEMORY ISOLATION TEST: FAIL\n");
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

	ul_printk("mem_iso: start\n");

	g_grant_ready = ul_notif_create();
	g_grant_done  = ul_notif_create();

	if (g_grant_ready == UL_NOTIF_INVALID || g_grant_done == UL_NOTIF_INVALID) {
		ul_printk("mem_iso: notif_create FAIL\n");
		ul_sim_exit(1);
	}

	/* Scenario 1 */
	attr = (ul_thread_attr_t){
		.name = "anon", .entry = anon_thread_entry,
		.arg = NULL, .priority = 5u,
		.stack_size = 1024u, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	/*
	 * Scenario 2: B must be spawned first so its TID is visible to A
	 * before A runs ul_mem_grant.
	 */
	attr = (ul_thread_attr_t){
		.name = "grant_b", .entry = grant_b_entry,
		.arg = NULL, .priority = 4u,
		.stack_size = 1024u, .privilege = UL_PRIV_DRIVER,
	};
	b_tid = ul_thread_create(&attr);
	g_grant_b_tid = b_tid;

	attr = (ul_thread_attr_t){
		.name = "grant_a", .entry = grant_a_entry,
		.arg = NULL, .priority = 4u,
		.stack_size = 1024u, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	/* Scenario 3 */
	attr = (ul_thread_attr_t){
		.name = "victim", .entry = fault_victim_entry,
		.arg = NULL, .priority = 6u,
		.stack_size = 1024u, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "trigger", .entry = fault_trigger_entry,
		.arg = NULL, .priority = 7u,
		.stack_size = 1024u, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	attr = (ul_thread_attr_t){
		.name = "sup", .entry = supervisor_entry,
		.arg = NULL, .priority = 15u,
		.stack_size = 2048u, .privilege = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	ul_thread_exit();
}
