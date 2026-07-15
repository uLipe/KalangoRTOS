/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static volatile uint32_t g_cpu0;
static volatile uint32_t g_cpu1;
static ulmk_notif_t g_done;

static void hammer1(void *arg)
{
	volatile uint32_t x = 0u;
	uint32_t i;

	(void)arg;
	for (i = 0u; i < 500u; i++) {
		x += i;
		ulmk_thread_yield();
	}
	(void)x;
	g_cpu1 = ulmk_cpu_id();
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
	board_console_puts("smp_vs_up: begin\n");

	g_cpu0 = ulmk_cpu_id();
	g_done = ulmk_notif_create();

	attr.name = "h1";
	attr.entry = hammer1;
	attr.priority = 1u;
	attr.stack_size = 2048u;
	attr.privilege = ULMK_PRIV_DRIVER;
	attr.cpu = 1u;
	ulmk_thread_create(&attr);

	for (i = 0u; i < 100000u && g_cpu1 == 0u; i++)
		ulmk_thread_yield();

	ulmk_notif_wait(g_done, 0x1u, &bits);
	if (g_cpu0 != 0u || g_cpu1 != 1u) {
		board_console_puts("smp_vs_up: FAIL\n");
		for (;;)
			;
	}

	board_console_puts("smp_vs_up: report dual-cpu hammer ok\n");
	board_console_puts("smp_vs_up: PASS\n");
	ulmk_thread_exit();
}
