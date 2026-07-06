/* Stub ulmk_arch.h for host-side unit tests — provides just enough to compile sched + bitmap_rt */
#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct { uint32_t pcxi; uint32_t csa_tail; } ulmk_arch_ctx_t;

/* IRQ control — no-op stubs for single-threaded host tests */
typedef uint32_t ulmk_arch_irq_key_t;
static inline ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void)       { return 0u; }
static inline void ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t k)  { (void)k; }
static inline void ulmk_arch_cpu_irq_enable(void)                   { }
static inline void ulmk_arch_cpu_irq_disable(void)                  { }

/* CLZ — GCC built-in equivalent for host x86 */
static inline uint32_t ulmk_arch_cpu_clz(uint32_t v)
{
	return v ? (uint32_t)__builtin_clz(v) : 32u;
}

/* MPU region descriptor */
#define ULMK_ARCH_MAX_REGIONS	12

typedef struct {
	uintptr_t base;
	size_t    size;
	uint32_t  perms;
	uint8_t   type;
} ulmk_arch_region_t;

#define ULMK_REGION_CODE		0
#define ULMK_REGION_DATA		1
#define ULMK_REGION_STACK		2
#define ULMK_REGION_HEAP		3
#define ULMK_REGION_PERIPH	4
#define ULMK_REGION_SHARED	5

/* Context — forward declarations; test file provides implementations */
void ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from, ulmk_arch_ctx_t *to);
void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t stack_top, int priv);
void ulmk_arch_ctx_free(ulmk_arch_ctx_t *ctx);
void ulmk_arch_mpu_switch(const ulmk_arch_region_t *regions, uint8_t count,
			uint8_t prs);

#define ULMK_SCHED_SWITCH_COOP		0u
#define ULMK_SCHED_SWITCH_PREEMPT_ISR	1u

static inline bool ulmk_arch_sched_isr_preempt_deferred(void)
{
	return false;
}

static inline void ulmk_arch_sched_switch(ulmk_arch_ctx_t *from,
					  const ulmk_arch_ctx_t *to,
					  unsigned int flags)
{
	(void)flags;

	ulmk_arch_ctx_switch(from, (ulmk_arch_ctx_t *)to);
}

#endif /* ULMK_ARCH_H */
