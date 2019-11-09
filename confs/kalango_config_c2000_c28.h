#pragma once

/*
 * KalangoRTOS — C28x architecture configuration template.
 *
 * Adapt these values to your specific F28x device and application.
 */

/* Select C28x architecture variant */
#define CONFIG_ARCH_C2000_C28               1

/* 8-byte stack alignment required by C28x EABI */
#define CONFIG_ARCH_ALIGNMENT_BYTES         4

/* System clock in Hz — adjust for your device and PLL settings */
#define CONFIG_PLATFORM_SYS_CLOCK_HZ        200000000U

/* Tick rate */
#define CONFIG_TICKS_PER_SEC                1000U

/* CPU Timer 2 base address (16-bit word address; standard across F28x) */
/* Default 0x0C00 is F28x standard; override if your device differs */
/* #define CONFIG_ARCH_CPUTIMER2_BASE       0x00000C00U */

/* PIE controller base address (16-bit word address; standard across F28x) */
/* #define CONFIG_ARCH_PIECTRL_BASE         0x00000CE0U */

/* Number of priority levels */
#define CONFIG_PRIORITY_LEVELS              32

/* Maximum number of tasks */
#define CONFIG_MAX_TASK_COUNT               16

/* Kernel heap size */
#define CONFIG_HEAP_SIZE                    (16 * 1024U)

/* IRQ table size: 16 (CPU INTs) + 96 (PIE) = 112 slots */
#define CONFIG_ISR_TABLE_SIZE               112U

/* Enable softirq subsystem */
#define CONFIG_ENABLE_SOFTIRQ               1
#define CONFIG_SOFTIRQ_MAX_VECTORS          8U

/* Enable debug */
#define CONFIG_DEBUG_KERNEL                 1

/* FPU support: set to 1 if your C28x device has FPU32 */
/* #define CONFIG_HAS_FLOAT                 1 */
