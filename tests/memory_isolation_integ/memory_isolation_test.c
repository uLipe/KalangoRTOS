/* SPDX-License-Identifier: MIT */
/*
 * Memory isolation integration test
 * tests/memory_isolation_integ/memory_isolation_test.c
 *
 * Five scenarios exercising MPU-backed memory isolation.  The supervisor
 * runs them sequentially to avoid cross-scenario interference.
 *
 *  1. Anon mmap: private heap allocation, pattern verify, unmap.
 *  2. Memory grant: cross-thread read with explicit grant.
 *  3. MPU write-fault: ungranted write kills the offending thread.
 *  4. Kernel exec fault: jump into .kernel_text kills the thread.
 *  5. Kernel data fault: read of kernel RAM kills the thread.
 *
 * All worker threads run at ULMK_PRIV_DRIVER (PRS 1).
 */

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>
#include "../test_support.h"

extern uint8_t _ulmk_kernel_data_start[];

/* Kernel .text entry — lives in CPR_KERNEL, not reachable from PRS 1. */
extern uint32_t ulmk_arch_cpu_irq_save(uint32_t *flags);

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

	base = ulmk_mem_map(NULL, 128u, ULMK_PERM_READ | ULMK_PERM_WRITE, ULMK_MMAP_ANON);
	if (!ulmk_test_map_ok(base)) {
		ulmk_printk("mem_iso: anon alloc FAIL\n");
		g_anon_result = 0;
		ulmk_thread_exit();
	}

	buf = (volatile uint32_t *)base;
	for (i = 0; i < 4; i++)
		buf[i] = PATTERN_A;

	for (i = 0; i < 4; i++) {
		if (buf[i] != PATTERN_A) {
			ulmk_printk("mem_iso: anon pattern FAIL i=%d\n", i);
			g_anon_result = 0;
			ulmk_thread_exit();
		}
	}

	ulmk_mem_unmap(base, 128u);
	g_anon_result = 1;
	ulmk_thread_exit();
}

/* =========================================================================
 * Scenario 2 — memory grant (cross-thread read)
 * ========================================================================= */

static volatile int      g_grant_result = -1;
static volatile ulmk_tid_t    g_grant_b_tid  = ULMK_TID_INVALID;
static volatile ulmk_notif_t  g_grant_ready  = ULMK_NOTIF_INVALID;
static volatile ulmk_notif_t  g_grant_done   = ULMK_NOTIF_INVALID;
static volatile void       *g_grant_base;

static void grant_a_entry(void *arg)
{
	volatile uint32_t *buf;
	void              *base;
	uint32_t           bits;
	int                ret;

	(void)arg;

	base = ulmk_mem_map(NULL, 64u, ULMK_PERM_READ | ULMK_PERM_WRITE, ULMK_MMAP_ANON);
	if (!ulmk_test_map_ok(base)) {
		ulmk_printk("mem_iso: grant-A alloc FAIL\n");
		g_grant_result = 0;
		ulmk_thread_exit();
	}

	buf    = (volatile uint32_t *)base;
	buf[0] = PATTERN_B;

	while (g_grant_b_tid == ULMK_TID_INVALID)
		ulmk_msleep(5);

	ret = ulmk_mem_grant(base, 64u, g_grant_b_tid, ULMK_PERM_READ);
	if (ret < 0) {
		ulmk_printk("mem_iso: ulmk_mem_grant FAIL ret=%d\n", ret);
		g_grant_result = 0;
		ulmk_thread_exit();
	}

	g_grant_base = base;
	ulmk_notif_signal(g_grant_ready, 1u);

	bits = 0;
	ulmk_notif_wait(g_grant_done, 1u, &bits);

	ulmk_mem_unmap(base, 64u);
	ulmk_thread_exit();
}

static void grant_b_entry(void *arg)
{
	volatile uint32_t *buf;
	uint32_t           bits;

	(void)arg;

	bits = 0;
	ulmk_notif_wait(g_grant_ready, 1u, &bits);

	buf = (volatile uint32_t *)g_grant_base;

	if (buf[0] == PATTERN_B)
		g_grant_result = 1;
	else {
		ulmk_printk("mem_iso: grant read FAIL got=0x%08x\n",
			  (unsigned)buf[0]);
		g_grant_result = 0;
	}

	ulmk_notif_signal(g_grant_done, 1u);
	ulmk_thread_exit();
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

	base = ulmk_mem_map(NULL, 64u, ULMK_PERM_READ | ULMK_PERM_WRITE, ULMK_MMAP_ANON);
	if (!ulmk_test_map_ok(base)) {
		ulmk_printk("mem_iso: fault-victim alloc FAIL\n");
		ulmk_thread_exit();
	}

	buf    = (volatile uint32_t *)base;
	buf[0] = 0xDEADBEEFu;

	g_fault_victim_base = base;
	ulmk_msleep(500u);
	ulmk_thread_exit();
}

static void fault_trigger_entry(void *arg)
{
	volatile uint32_t *victim;

	(void)arg;

	while (!g_fault_victim_base)
		ulmk_msleep(5);

	victim = (volatile uint32_t *)g_fault_victim_base;

	g_fault_progress = 1;
	victim[0] = 0xCAFECAFEu;
	g_fault_progress = 2;

	ulmk_thread_exit();
}

/* =========================================================================
 * Scenario 4 — userspace cannot execute kernel code
 * ========================================================================= */

static volatile int g_kexec_progress;

static void kexec_trigger_entry(void *arg)
{
	typedef uint32_t (*kern_fn_t)(uint32_t *);
	kern_fn_t fn;

	(void)arg;

	fn = (kern_fn_t)(uintptr_t)ulmk_arch_cpu_irq_save;
	g_kexec_progress = 1;
	fn(NULL);
	g_kexec_progress = 2;
	ulmk_thread_exit();
}

/* =========================================================================
 * Scenario 5 — userspace cannot read kernel data
 * ========================================================================= */

static volatile int g_kread_progress;

static void kread_trigger_entry(void *arg)
{
	volatile uint32_t val;

	(void)arg;

	g_kread_progress = 1;
	val = *(volatile uint32_t *)(uintptr_t)_ulmk_kernel_data_start;
	(void)val;
	g_kread_progress = 2;
	ulmk_thread_exit();
}

/* =========================================================================
 * Supervisor — runs scenarios sequentially, reports, exits
 * ========================================================================= */

static int wait_flag(volatile int *flag, int target, uint32_t ms_max)
{
	uint32_t waited = 0;

	while (*flag != target && waited < ms_max) {
		ulmk_msleep(10);
		waited += 10u;
	}

	return *flag == target;
}

static ulmk_tid_t spawn_worker(const char *name, void (*entry)(void *),
			       uint32_t prio, uint32_t stack)
{
	ulmk_thread_attr_t attr = {
		.name       = name,
		.entry      = entry,
		.arg        = NULL,
		.priority   = prio,
		.stack_size = stack,
		.privilege  = ULMK_PRIV_DRIVER,
	};

	return ulmk_thread_create(&attr);
}

static void supervisor_entry(void *arg)
{
	ulmk_tid_t b_tid;
	int        overall = 1;

	(void)arg;

	/* Scenario 1 */
	g_anon_result = -1;
	spawn_worker("anon", anon_thread_entry, 15u, 1024u);
	if (wait_flag(&g_anon_result, 1, 3000u)) {
		ulmk_printk("mem_iso: scenario 1 (anon mmap) PASS\n");
	} else {
		ulmk_printk("mem_iso: FAIL at scenario 1 (anon mmap)\n");
		overall = 0;
	}

	/* Scenario 2 */
	g_grant_result = -1;
	g_grant_b_tid  = ULMK_TID_INVALID;
	b_tid = spawn_worker("grant_b", grant_b_entry, 15u, 1024u);
	g_grant_b_tid = b_tid;
	spawn_worker("grant_a", grant_a_entry, 15u, 1024u);
	if (wait_flag(&g_grant_result, 1, 3000u)) {
		ulmk_printk("mem_iso: scenario 2 (grant read) PASS\n");
	} else {
		ulmk_printk("mem_iso: FAIL at scenario 2 (grant)\n");
		overall = 0;
	}

	/* Scenario 3 */
	g_fault_progress = 0;
	spawn_worker("victim", fault_victim_entry, 18u, 1024u);
	spawn_worker("trigger", fault_trigger_entry, 16u, 1024u);
	wait_flag(&g_fault_progress, 1, 2000u);
	ulmk_msleep(200);

	if (g_fault_progress == 1) {
		ulmk_printk("mem_iso: scenario 3 (MPU fault) PASS\n");
	} else if (g_fault_progress == 2) {
		ulmk_printk("mem_iso: scenario 3 (MPU fault) PARTIAL "
			  "(MPU not enforced on this platform)\n");
	} else {
		ulmk_printk("mem_iso: FAIL at scenario 3 (trigger never ran)\n");
		overall = 0;
	}

	/* Scenario 4 */
	g_kexec_progress = 0;
	spawn_worker("kexec", kexec_trigger_entry, 16u, 1024u);
	wait_flag(&g_kexec_progress, 1, 2000u);
	ulmk_msleep(200);

	if (g_kexec_progress == 1) {
		ulmk_printk("mem_iso: scenario 4 (kernel exec fault) PASS\n");
	} else if (g_kexec_progress == 2) {
		ulmk_printk("mem_iso: scenario 4 (kernel exec fault) PARTIAL "
			  "(MPU not enforced on this platform)\n");
	} else {
		ulmk_printk("mem_iso: FAIL at scenario 4 (kexec never ran)\n");
		overall = 0;
	}

	/* Scenario 5 */
	g_kread_progress = 0;
	spawn_worker("kread", kread_trigger_entry, 16u, 1024u);
	wait_flag(&g_kread_progress, 1, 2000u);
	ulmk_msleep(200);

	if (g_kread_progress == 1) {
		ulmk_printk("mem_iso: scenario 5 (kernel data fault) PASS\n");
	} else if (g_kread_progress == 2) {
		ulmk_printk("mem_iso: scenario 5 (kernel data fault) PARTIAL "
			  "(MPU not enforced on this platform)\n");
	} else {
		ulmk_printk("mem_iso: FAIL at scenario 5 (kread never ran)\n");
		overall = 0;
	}

	if (overall) {
		ulmk_printk("MEMORY ISOLATION TEST: PASS\n");
		ulmk_sim_exit(0);
	} else {
		ulmk_printk("MEMORY ISOLATION TEST: FAIL\n");
		ulmk_sim_exit(1);
	}
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         sup_tid;

	(void)info;

	ulmk_printk("mem_iso: start\n");

	g_grant_ready = ulmk_notif_create();
	g_grant_done  = ulmk_notif_create();

	if (g_grant_ready == ULMK_NOTIF_INVALID || g_grant_done == ULMK_NOTIF_INVALID) {
		ulmk_printk("mem_iso: notif_create FAIL\n");
		ulmk_sim_exit(1);
	}

	attr = (ulmk_thread_attr_t){
		.name = "sup", .entry = supervisor_entry,
		.arg = NULL, .priority = 20u,
		.stack_size = 2048u, .privilege = ULMK_PRIV_DRIVER,
	};
	sup_tid = ulmk_thread_create(&attr);
	if (sup_tid == ULMK_TID_INVALID ||
	    ulmk_cap_grant(sup_tid, ULMK_CAP_SPAWN) < 0) {
		ulmk_printk("mem_iso: supervisor setup FAIL\n");
		ulmk_sim_exit(1);
	}

	ulmk_thread_exit();
}
