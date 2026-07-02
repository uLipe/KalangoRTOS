/* Stub config for unit tests — values mirror test Makefile -D flags */
#ifndef ULMK_CONFIG_H
#define ULMK_CONFIG_H

#define ULMK_CONFIG_MAX_THREADS      16
#define ULMK_CONFIG_MAX_ENDPOINTS    16
#define ULMK_CONFIG_MAX_NOTIFS       16
#define ULMK_CONFIG_MAX_IRQ_BINDINGS 16
#define ULMK_CONFIG_TICK_HZ          1000
#define ULMK_CONFIG_DEBUG_PRINTK     0

#define ULMK_CONFIG_HW_SYS_CLOCK_HZ    50000000

#endif /* ULMK_CONFIG_H */

#define ULMK_CONFIG_SCHED_QUANTUM_US    10000
#define ULMK_CONFIG_SCHED_QUANTUM_TICKS 10
