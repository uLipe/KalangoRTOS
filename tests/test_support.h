/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * tests/test_support.h — simulator-agnostic test utilities
 *
 * Each board/simulator provides a strong implementation of ulmk_sim_exit().
 * Integration tests call ulmk_sim_exit(0) to signal success to the simulator.
 */

#ifndef UL_TEST_SUPPORT_H
#define UL_TEST_SUPPORT_H

#include <stdint.h>
#include <ulmk/microkernel.h>

/*
 * ulmk_sim_exit — terminate the simulation and report pass (0) or fail (!=0).
 *
 * QEMU (boards/qemu_tc3xx/qemu_console.c): writes to the VIRT exit register.
 * TSIM (boards/tsim_tc39x/tsim_console.c): sets A14=0x900d and calls debug.
 */
extern void ulmk_sim_exit(int code) __attribute__((noreturn));

void board_timer_start(const ulmk_boot_info_t *info);
void board_timer_sleep_us(uint32_t us);

/*
 * ulmk_msleep — test-only delay helper.
 *
 * Integration tests use ulmk_msleep() for ordering (brief delays to ensure
 * one thread is blocked before another proceeds).  The kernel no longer
 * provides a sleep syscall; instead, userspace should use the timer server.
 *
 * For tests that only need ordering (not precise timing), a yield-loop is
 * sufficient: each yield gives every ready thread a scheduling opportunity,
 * and the number of yields scales with the requested duration so that
 * longer sleeps give more opportunities.  Tests that require wall-clock
 * accuracy should use the timer server directly.
 */
static inline void ulmk_msleep(uint32_t ms)
{
	uint32_t i;

	for (i = 0u; i < ms * 20u; i++)
		ulmk_thread_yield();
}

#endif /* UL_TEST_SUPPORT_H */
