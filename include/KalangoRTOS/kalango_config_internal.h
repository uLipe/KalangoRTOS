#ifndef __KALANGO_CONFIG_INTERNAL_H
#define __KALANGO_CONFIG_INTERNAL_H

#include <kalango_config.h>

#ifndef CONFIG_TICKS_PER_SEC
#define CONFIG_TICKS_PER_SEC                1000
#endif

#ifndef CONFIG_KERNEL_HEAP_SIZE
#define CONFIG_KERNEL_HEAP_SIZE          (2 * 1024)
#endif

#ifndef CONFIG_PLATFORM_SYS_CLOCK_HZ
#define CONFIG_PLATFORM_SYS_CLOCK_HZ     64000000
#endif

#ifndef CONFIG_KERNEL_BLOCKS
#define CONFIG_KERNEL_BLOCKS 16
#endif

#ifndef CONFIG_REMOVE_CHECKINGS
#define CONFIG_REMOVE_CHECKINGS               0
#endif

#ifndef CONFIG_ENABLE_TASKS
#define CONFIG_ENABLE_TASKS                   1
#endif

#ifndef CONFIG_DEBUG_KERNEL
#define CONFIG_DEBUG_KERNEL                   0
#endif

#ifndef CONFIG_ENABLE_SEMAPHORES
#define CONFIG_ENABLE_SEMAPHORES              1
#endif

#ifndef CONFIG_ENABLE_MUTEXES
#define CONFIG_ENABLE_MUTEXES                 1
#endif

#ifndef CONFIG_ENABLE_QUEUES
#define CONFIG_ENABLE_QUEUES                  1
#endif

#ifndef CONFIG_ENABLE_TIMERS
#define CONFIG_ENABLE_TIMERS                  1
#endif

#ifndef CONFIG_PRIORITY_LEVELS
#define CONFIG_PRIORITY_LEVELS              16
#endif

#ifndef CONFIG_MUTEX_CEIL_PRIORITY
#define CONFIG_MUTEX_CEIL_PRIORITY          (CONFIG_PRIORITY_LEVELS - 1)
#endif

#ifndef CONFIG_IDLE_TASK_STACK_SIZE
#define CONFIG_IDLE_TASK_STACK_SIZE         128
#endif

#ifndef CONFIG_ISR_STACK_SIZE
#define CONFIG_ISR_STACK_SIZE               128
#endif

#ifndef CONFIG_USE_PLATFORM_INIT
#define CONFIG_USE_PLATFORM_INIT            0
#endif

#ifndef CONFIG_ARCH_ARM_V7M
#define CONFIG_ARCH_ARM_V7M                 0
#endif

#if (CONFIG_ARCH_ARM_V7M > 0)
#ifndef CONFIG_HAS_FLOAT
#define CONFIG_HAS_FLOAT                    0
#endif
#endif

#ifndef CONFIG_ARCH_ARM_V6M
#define CONFIG_ARCH_ARM_V6M                 0
#endif

#ifndef CONFIG_ARCH_C2000
#define CONFIG_ARCH_C2000                   0
#endif

#if (CONFIG_ARCH_C2000 > 0)
#ifndef CONFIG_HAS_FLOAT
#define CONFIG_HAS_FLOAT                    0
#endif
#endif

#ifndef CONFIG_ARCH_ALIGNMENT_BYTES
#define CONFIG_ARCH_ALIGNMENT_BYTES         4
#endif

#endif