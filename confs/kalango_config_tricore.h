#ifndef KALANGO_CONFIG_H
#define KALANGO_CONFIG_H

/* TriCore TC1.6 / AURIX TC2xx / TC3xx target */
#define CONFIG_ARCH_TRICORE              1

/* System clock — TC27x runs at 200 MHz by default (adjust for your target) */
#define CONFIG_PLATFORM_SYS_CLOCK_HZ    200000000UL

/* Kernel tick */
#define CONFIG_TICKS_PER_SEC            1000

/* Task scheduling */
#define CONFIG_PRIORITY_LEVELS          16
#define CONFIG_ENABLE_ROUND_ROBIN_SCHED 1

/* Memory */
#define CONFIG_KERNEL_HEAP_SIZE         (32 * 1024)
#define CONFIG_KERNEL_BLOCKS            32

/* Stack sizes (bytes) */
#define CONFIG_ISR_STACK_SIZE           512
#define CONFIG_MAIN_TASK_STACK_SIZE     2048
#define CONFIG_IDLE_TASK_STACK_SIZE     256
#define CONFIG_SOFTIRQ_TASK_STACK_SIZE  1024
#define CONFIG_MAIN_TASK_PRIORITY       (CONFIG_PRIORITY_LEVELS - 1)

/* IRQ */
#define CONFIG_IRQ_PRIORITY_LEVELS      256   /* TriCore supports 0..255 */
#define CONFIG_ISR_TABLE_SIZE           256

/* CSA free list pool — each task uses 3 CSAs (lower + inner_upper + outer_upper);
 * each interrupt nesting level uses 2 temporary CSAs.
 * Size = (max_concurrent_tasks * 3) + (max_isr_nesting * 2) + margin. */
#define CONFIG_TRICORE_CSA_COUNT        128

/* Interrupt priorities (must not overlap) */
#define ARCH_TRICORE_SW_IRQ_PRIORITY    1
#define ARCH_TRICORE_TICK_PRIORITY      2

/* Softirq */
#define CONFIG_ENABLE_SOFTIRQ           1
#define CONFIG_SOFTIRQ_MAX_VECTORS      8

/* Stack alignment */
#define CONFIG_ARCH_ALIGNMENT_BYTES     8

/* Platform init is called from startup.S before main(); no kernel callback needed */
#define CONFIG_USE_PLATFORM_INIT        0

#endif /* KALANGO_CONFIG_H */
