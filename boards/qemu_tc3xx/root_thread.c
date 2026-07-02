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
#include <ul/microkernel.h>
#include "console.h"

static void counter_entry(void *arg)
{
	uint32_t count = 0;

	(void)arg;

	console_puts("ulipeMicroKernel: counter thread started\n");

	while (1) {
		console_printf("ulipeMicroKernel: hello from QEMU — tick #%u\n",
			       (unsigned)count++);
		ul_msleep(1000);
	}
}

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	(void)info;

	console_init();
	console_puts("ulipeMicroKernel: root thread start\n");

	attr = (ul_thread_attr_t){
		.name       = "counter",
		.entry      = counter_entry,
		.arg        = NULL,
		.priority   = 5u,
		.stack_size = 2048u,
		.privilege  = UL_PRIV_DRIVER,
	};
	ul_thread_create(&attr);

	ul_thread_exit();
}
