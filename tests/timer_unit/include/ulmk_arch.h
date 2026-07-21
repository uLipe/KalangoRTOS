/* Stub ulmk_arch.h for timer_unit */
#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H

#include <stdint.h>

#ifndef ULMK_ARCH_NUM_CPU
#define ULMK_ARCH_NUM_CPU	1
#endif

#ifndef ULMK_ARCH_MAX_REGIONS
#define ULMK_ARCH_MAX_REGIONS	12
#endif

typedef struct { uintptr_t pc; uintptr_t sp; } ulmk_arch_ctx_t;
typedef uint32_t ulmk_arch_irq_key_t;
typedef struct { volatile uint32_t locked; } ulmk_spinlock_t;
#define ULMK_SPINLOCK_INIT	{ 0u }

static inline uint32_t ulmk_arch_cpu_id(void)
{
	return 0u;
}

static inline uint32_t ulmk_arch_timer_wheel_cpu(void)
{
	return 0u;
}

#endif /* ULMK_ARCH_H */
