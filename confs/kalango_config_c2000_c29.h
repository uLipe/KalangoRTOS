#ifndef __KALANGO_CONFIG_H
#define __KALANGO_CONFIG_H

#define CONFIG_TICKS_PER_SEC                1000

#define CONFIG_ENABLE_TASKS                   1
#define CONFIG_ENABLE_SEMAPHORES              1
#define CONFIG_ENABLE_MUTEXES                 1
#define CONFIG_ENABLE_QUEUES                  1
#define CONFIG_ENABLE_TIMERS                  1

#define CONFIG_KERNEL_HEAP_SIZE          16 * 1024

#define CONFIG_PRIORITY_LEVELS              16
#define CONFIG_MUTEX_CEIL_PRIORITY          (CONFIG_PRIORITY_LEVELS - 1)
#define CONFIG_IDLE_TASK_STACK_SIZE         512
#define CONFIG_ISR_STACK_SIZE               1024

#define CONFIG_USE_PLATFORM_INIT            0

#define CONFIG_ARCH_C2000                   1
#define CONFIG_ARCH_ALIGNMENT_BYTES         8

#define CONFIG_PLATFORM_SYS_CLOCK_HZ     200000000
#define CONFIG_DEBUG_KERNEL              1

#endif