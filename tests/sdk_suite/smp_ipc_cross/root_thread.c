/* SPDX-License-Identifier: MIT */
/*
 * Cross-CPU wake path used by IPC reply (remote enqueue + IPI + notif).
 * Full ep_call/reply rendezvous under ISR preemption needs per-hart IRQ
 * stacks — tracked separately from the SMP overnight MVP.
 */
#include <stdint.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static volatile uint32_t g_seen;
static ulmk_notif_t g_done;

static void worker_cpu1(void *arg)
{
	(void)arg;
	g_seen = ulmk_cpu_id();
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
	board_console_puts("smp_ipc_cross: begin\n");

	g_done = ulmk_notif_create();
	attr.name = "w1";
	attr.entry = worker_cpu1;
	attr.priority = 1u;
	attr.stack_size = 2048u;
	attr.privilege = ULMK_PRIV_DRIVER;
	attr.cpu = 1u;
	if (ulmk_thread_create(&attr) == ULMK_TID_INVALID) {
		board_console_puts("smp_ipc_cross: FAIL spawn\n");
		for (;;)
			;
	}

	for (i = 0u; i < 100000u && g_seen == 0u; i++)
		ulmk_thread_yield();

	ulmk_notif_wait(g_done, 0x1u, &bits);
	if (g_seen != 1u) {
		board_console_puts("smp_ipc_cross: FAIL\n");
		for (;;)
			;
	}
	board_console_puts("smp_ipc_cross: PASS\n");
	ulmk_thread_exit();
}
