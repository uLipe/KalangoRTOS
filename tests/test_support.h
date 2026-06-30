/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * tests/test_support.h — simulator-agnostic test utilities
 *
 * Each board/simulator provides a strong implementation of ul_sim_exit().
 * Integration tests call ul_sim_exit(0) to signal success to the simulator.
 */

#ifndef UL_TEST_SUPPORT_H
#define UL_TEST_SUPPORT_H

#include <stdint.h>

/*
 * ul_sim_exit — terminate the simulation and report pass (0) or fail (!=0).
 *
 * QEMU (boards/qemu_tc27x/qemu_console.c): writes to the VIRT exit register.
 * TSIM (boards/tsim_tc27x/tsim_console.c): sets A14=0x900d and calls debug.
 */
extern void ul_sim_exit(int code) __attribute__((noreturn));

#endif /* UL_TEST_SUPPORT_H */
