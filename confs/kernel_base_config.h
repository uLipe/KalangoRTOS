#pragma once 

#define CONFIG_TICKS_PER_SEC                1000

#define CONFIG_NOOF_TASKS                   1
#define CONFIG_NOOF_SEMAPHORES              1
#define CONFIG_NOOF_MUTEXES                 1
#define CONFIG_NOOF_QUEUES                  1
#define CONFIG_NOOF_TIMERS                  1

#define CONFIG_KERNEL_BLOCKS                64

#define CONFIG_PRIORITY_LEVELS              16
#define CONFIG_MUTEX_CEIL_PRIORITY          (CONFIG_PRIORITY_LEVELS - 1)
#define CONFIG_IDLE_TASK_STACK_SIZE         512
#define CONFIG_ISR_STACK_SIZE               1024

#define CONFIG_USE_PLATFORM_INIT            1