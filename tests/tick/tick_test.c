/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Tick integration test — tests/tick/tick_test.c
 *
 * Root thread initialises the STM0 tickless timer and fires 50 one-shot
 * CMP0 deadlines.  Each deadline fires _arch_tick_isr_handler which
 * increments the internal tick counter.  The root thread monitors the
 * counter, re-arms the next deadline after each interrupt, and idles
 * (TriCore WAIT) between deadlines so QEMU can fast-forward to the
 * next timer event.  On reaching 50 counts the test prints the PASS
 * sentinel and returns to idle.
 *
 * QEMU is not real-time; the elapsed µs reported by ul_arch_tick_get()
 * will not match wall-clock time.  Only the count is verified.
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <ul_arch.h>
#include <kernel/include/ul_printk.h>

extern void qemu_virt_exit(uint32_t code);

#define TICK_TARGET	50u
#define DEADLINE_US	500u

void ul_root_thread(const ul_boot_info_t *info)
{
	(void)info;

	ul_arch_tick_init();
	ul_arch_cpu_irq_enable();
	ul_arch_tick_deadline(DEADLINE_US);

	uint32_t prev = 0u;

	while (ul_arch_tick_count() < TICK_TARGET) {
		ul_arch_cpu_idle();

		uint32_t cnt = ul_arch_tick_count();
		if (cnt != prev) {
			prev = cnt;
			if (cnt < TICK_TARGET)
				ul_arch_tick_deadline(DEADLINE_US);
		}
	}

	ul_printk("tick_test: PASS (50 ticks counted)\n");
	qemu_virt_exit(0);
}
