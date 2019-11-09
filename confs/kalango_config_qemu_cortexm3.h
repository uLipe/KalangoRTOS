#pragma once

#define CONFIG_TICKS_PER_SEC                1000

#define CONFIG_ENABLE_TASKS                   1
#define CONFIG_ENABLE_SEMAPHORES              1
#define CONFIG_ENABLE_MUTEXES                 1
#define CONFIG_ENABLE_QUEUES                  1
#define CONFIG_ENABLE_TIMERS                  1
#define CONFIG_ENABLE_ROUND_ROBIN_SCHED       1

#define CONFIG_KERNEL_HEAP_SIZE          (32 * 1024)
#define CONFIG_KERNEL_BLOCKS                  32

#define CONFIG_PRIORITY_LEVELS              16
#define CONFIG_MUTEX_CEIL_PRIORITY          (CONFIG_PRIORITY_LEVELS - 1)
#define CONFIG_IDLE_TASK_STACK_SIZE         512
#define CONFIG_ISR_STACK_SIZE               1024

#define CONFIG_USE_PLATFORM_INIT            0
#define CONFIG_REMOVE_CHECKINGS             0
#define CONFIG_DEBUG_KERNEL                 1

#define CONFIG_ARCH_ARM_V7M                 1
#define CONFIG_HAS_FLOAT                    0
#define CONFIG_IRQ_PRIORITY_LEVELS          8

/* QEMU SysTick with CLKSOURCE=1 uses a fixed 1 MHz reference in qemu-system-arm. */
#define CONFIG_PLATFORM_SYS_CLOCK_HZ        1000000
