/* SPDX-License-Identifier: MIT */
/*
 * Default kernel configuration for the QEMU TC3xx board.
 * boards/qemu_tc3xx/ul/config.h
 *
 * Every symbol is guarded with #ifndef so individual values can be
 * overridden at compile time by passing -DUL_CONFIG_FOO=value.
 */
#ifndef UL_CONFIG_H
#define UL_CONFIG_H

/* Maximum number of IRQ→notification bindings in the static table. */
#ifndef UL_CONFIG_MAX_IRQ_BINDINGS
#define UL_CONFIG_MAX_IRQ_BINDINGS	16
#endif

/* Scheduler tick rate in Hz. */
#ifndef UL_CONFIG_TICK_HZ
#define UL_CONFIG_TICK_HZ		1000
#endif

/* Enable kernel debug printk output. */
#ifndef UL_CONFIG_DEBUG_PRINTK
#define UL_CONFIG_DEBUG_PRINTK		1
#endif

/* Scheduler preemption quantum in microseconds. */
#ifndef UL_CONFIG_SCHED_QUANTUM_US
#define UL_CONFIG_SCHED_QUANTUM_US	10000
#endif

/*
 * Quantum in ticks — must equal UL_CONFIG_SCHED_QUANTUM_US / (1e6 / TICK_HZ).
 * With TICK_HZ=1000 and QUANTUM_US=10000: 10000 / 1000 = 10 ticks.
 */
#ifndef UL_CONFIG_SCHED_QUANTUM_TICKS
#define UL_CONFIG_SCHED_QUANTUM_TICKS	10
#endif

/* System clock frequency used to program the tick timer period. */
#ifndef UL_CONFIG_HW_SYS_CLOCK_HZ
#define UL_CONFIG_HW_SYS_CLOCK_HZ	50000000
#endif

#endif /* UL_CONFIG_H */
