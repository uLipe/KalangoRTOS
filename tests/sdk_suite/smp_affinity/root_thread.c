/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <ulmk/microkernel.h>
#include <board_services.h>
#include <board_console.h>

static volatile uint32_t g_samples[32];
static volatile uint32_t g_n;
static volatile uint32_t g_pin1_started;
static ulmk_notif_t g_done;

static void pin1(void *arg)
{
	uint32_t i;

	(void)arg;
	g_pin1_started = 1u;
	for (i = 0u; i < 32u; i++) {
		g_samples[i] = ulmk_cpu_id();
		ulmk_thread_yield();
	}
	g_n = 32u;
	ulmk_notif_signal(g_done, 0x1u);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	uint32_t bits = 0;
	uint32_t i;
	ulmk_tid_t tid;

	(void)info;
	board_services_init(info);
	board_console_puts("smp_affinity: begin\n");

	if (ulmk_cpu_id() != 0u) {
		board_console_puts("smp_affinity: FAIL root cpu\n");
		for (;;)
			;
	}

	g_done = ulmk_notif_create();
	board_console_puts("smp_affinity: notif\n");

	attr.name = "pin1";
	attr.entry = pin1;
	attr.priority = 1u;
	attr.stack_size = 2048u;
	attr.privilege = ULMK_PRIV_DRIVER;
	attr.cpu = 1u;
	tid = ulmk_thread_create(&attr);
	if (tid == ULMK_TID_INVALID) {
		board_console_puts("smp_affinity: FAIL spawn\n");
		for (;;)
			;
	}
	board_console_puts("smp_affinity: spawned\n");

	for (i = 0u; i < 100000u && g_pin1_started == 0u; i++)
		ulmk_thread_yield();

	if (g_pin1_started == 0u) {
		board_console_puts("smp_affinity: FAIL pin1 never ran\n");
		for (;;)
			;
	}
	board_console_puts("smp_affinity: pin1 ok\n");

	ulmk_notif_wait(g_done, 0x1u, &bits);

	if (g_n != 32u) {
		board_console_puts("smp_affinity: FAIL short samples\n");
		for (;;)
			;
	}

	for (i = 0u; i < g_n; i++) {
		if (g_samples[i] != 1u) {
			board_console_puts("smp_affinity: FAIL migrated\n");
			for (;;)
				;
		}
	}
	board_console_puts("smp_affinity: PASS\n");
	ulmk_thread_exit();
}
