/* SPDX-License-Identifier: MIT */
/*
 * Default root thread for the QEMU TC3xx board.
 * boards/qemu_tc3xx/root_thread.c
 *
 * Spawns a counter thread that prints a hello message every second and
 * keeps the system running indefinitely — useful for quick smoke tests
 * with `dev.py build qemu`.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "console.h"
#include "board_timer.h"

static void counter_entry(void *arg)
{
	uint32_t count = 0;

	(void)arg;

	console_puts("ulmk: counter thread started\n");

	while (1) {
		console_printf("ulmk: hello from QEMU — tick #%u\n",
			       (unsigned)count++);
		board_timer_sleep_us(100000u);
	}
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};

	console_init();
	console_puts("ulmk: root thread start\n");

	board_timer_start(info);

	attr = (ulmk_thread_attr_t){
		.name       = "counter",
		.entry      = counter_entry,
		.arg        = NULL,
		.priority   = 5u,
		.stack_size = 2048u,
		.privilege  = ULMK_PRIV_DRIVER,
	};
	ulmk_thread_create(&attr);

	ulmk_thread_exit();
}
