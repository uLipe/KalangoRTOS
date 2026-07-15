/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static volatile uint32_t g_seen_cpu1;
static ulmk_notif_t g_done;

static void worker_cpu1(void *arg)
{
	(void)arg;
	g_seen_cpu1 = ulmk_cpu_id();
	ulmk_notif_signal(g_done, 0x1u);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	uint32_t bits = 0;
	uint32_t i;

	(void)info;
	board_services_init(info);
	board_console_puts("smp_smoke: begin\n");

	if (ulmk_cpu_id() != 0u) {
		board_console_puts("smp_smoke: FAIL root not on CPU0\n");
		for (;;)
			;
	}

	g_done = ulmk_notif_create();
	attr.name = "w1";
	attr.entry = worker_cpu1;
	attr.priority = 1u;
	attr.stack_size = 2048u;
	attr.privilege = ULMK_PRIV_DRIVER;
	attr.cpu = 1u;
	if (ulmk_thread_create(&attr) == ULMK_TID_INVALID) {
		board_console_puts("smp_smoke: FAIL spawn\n");
		for (;;)
			;
	}

	for (i = 0u; i < 100000u && g_seen_cpu1 == 0u; i++)
		ulmk_thread_yield();

	ulmk_notif_wait(g_done, 0x1u, &bits);

	if (g_seen_cpu1 != 1u) {
		board_console_puts("smp_smoke: FAIL cpu1 not seen\n");
		for (;;)
			;
	}

	board_console_puts("smp_smoke: PASS\n");
	ulmk_thread_exit();
}
