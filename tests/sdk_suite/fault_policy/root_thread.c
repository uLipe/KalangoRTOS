/* SPDX-License-Identifier: MIT */
/*
 * fault_policy — userspace MPU fault kills the guilty thread; system lives.
 *
 * 1) USER writes a page it does not own → killed
 * 2) DRIVER same → killed
 * 3) CAP_ALL DRIVER stand-in (root-equivalent rights) → killed
 *    Real ulmk_root_thread observes and prints PASS (does not suicide).
 */
#include "sdk_test_util.h"

#define BIT_GO		(1u << 0)
#define BIT_ACK		(1u << 1)
#define STACK_SZ	1024u

static ulmk_notif_t g_sync;
static volatile uint32_t *g_forbidden;
static volatile int g_user_armed;
static volatile int g_user_survived;
static volatile int g_drv_armed;
static volatile int g_drv_survived;
static volatile int g_root_armed;
static volatile int g_root_survived;

static void bad_write(void)
{
	g_forbidden[0] = 0xDEADBEEFu;
}

static void user_faulter(void *arg)
{
	uint32_t bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	g_user_armed = 1;
	ulmk_notif_signal(g_sync, BIT_ACK);
	bad_write();
	g_user_survived = 1;
	ulmk_thread_exit();
}

static void drv_faulter(void *arg)
{
	uint32_t bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	g_drv_armed = 1;
	ulmk_notif_signal(g_sync, BIT_ACK);
	bad_write();
	g_drv_survived = 1;
	ulmk_thread_exit();
}

static void root_faulter(void *arg)
{
	uint32_t bits = 0u;

	(void)arg;
	ulmk_notif_wait(g_sync, BIT_GO, &bits);
	g_root_armed = 1;
	ulmk_notif_signal(g_sync, BIT_ACK);
	bad_write();
	g_root_survived = 1;
	ulmk_thread_exit();
}

static int run_fault_case(void (*entry)(void *), ulmk_privilege_t priv,
			  volatile int *armed, volatile int *survived,
			  const char *tag, uint32_t caps)
{
	ulmk_tid_t tid;
	uint32_t   bits = 0u;
	int        i;
	int        dead;

	*armed = 0;
	*survived = 0;
	tid = sdk_spawn_priv(tag, entry, NULL, 20u, STACK_SZ, 0u, priv);
	if (tid == ULMK_TID_INVALID)
		return 0;
	if (caps != 0u && ulmk_cap_grant(tid, caps) != ULMK_OK)
		return 0;

	ulmk_notif_signal(g_sync, BIT_GO);
	bits = 0u;
	ulmk_notif_wait(g_sync, BIT_ACK, &bits);
	for (i = 0; i < 400; i++)
		ulmk_thread_yield();

	dead = (ulmk_thread_priority_get(tid) < 0) || (*armed && !*survived);
	sdk_puts("fault_policy: ");
	sdk_puts(tag);
	sdk_puts(dead && *armed && !*survived ? " PASS\n" : " FAIL\n");
	return dead && *armed && !*survived;
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	void *page;
	int   u_ok;
	int   d_ok;
	int   r_ok;

	board_services_init(info);
	sdk_puts("fault_policy: begin\n");

	g_sync = ulmk_notif_create();
	page = ulmk_mem_map(NULL, 128u, ULMK_PERM_READ | ULMK_PERM_WRITE,
			    ULMK_MMAP_ANON);
	if (!sdk_map_ok(page) || g_sync == ULMK_NOTIF_INVALID) {
		sdk_puts("fault_policy: FAIL\n");
		ulmk_thread_exit();
	}
	g_forbidden = (volatile uint32_t *)page;

	u_ok = run_fault_case(user_faulter, ULMK_PRIV_USER,
			      &g_user_armed, &g_user_survived, "user", 0u);
	d_ok = run_fault_case(drv_faulter, ULMK_PRIV_DRIVER,
			      &g_drv_armed, &g_drv_survived, "driver", 0u);
	r_ok = run_fault_case(root_faulter, ULMK_PRIV_DRIVER,
			      &g_root_armed, &g_root_survived, "root_caps",
			      ULMK_CAP_ALL);

	sdk_puts("fault_policy: u=");
	sdk_put_u32((uint32_t)u_ok);
	sdk_puts(" d=");
	sdk_put_u32((uint32_t)d_ok);
	sdk_puts(" r=");
	sdk_put_u32((uint32_t)r_ok);
	sdk_puts("\n");
	sdk_puts((u_ok && d_ok && r_ok) ? "fault_policy: PASS\n"
					: "fault_policy: FAIL\n");
	(void)ulmk_mem_unmap(page, 128u);
	ulmk_thread_exit();
}
