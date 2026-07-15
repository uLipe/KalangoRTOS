/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static ulmk_notif_t g_done;
static volatile uint32_t g_n;

static void worker_cpu1(void *arg)
{
	uint32_t i;

	(void)arg;
	for (i = 0u; i < 64u; i++) {
		if (ulmk_cpu_id() != 1u) {
			ulmk_notif_signal(g_done, 0x2u);
			ulmk_thread_exit();
		}
		ulmk_thread_yield();
	}
	g_n = 64u;
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
	board_console_puts("smp_stress: begin\n");
	g_done = ulmk_notif_create();

	attr.name = "w1";
	attr.entry = worker_cpu1;
	attr.priority = 1u;
	attr.stack_size = 2048u;
	attr.privilege = ULMK_PRIV_DRIVER;
	attr.cpu = 1u;
	ulmk_thread_create(&attr);

	for (i = 0u; i < 100000u && g_n == 0u; i++)
		ulmk_thread_yield();

	ulmk_notif_wait(g_done, 0x3u, &bits);
	if ((bits & 0x2u) || g_n != 64u) {
		board_console_puts("smp_stress: FAIL\n");
		for (;;)
			;
	}
	board_console_puts("smp_stress: PASS\n");
	ulmk_thread_exit();
}
