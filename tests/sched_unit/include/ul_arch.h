/* Stub ul_arch.h for host-side unit tests — provides just enough to compile sched + bitmap_rt */
#ifndef UL_ARCH_H
#define UL_ARCH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct { uint32_t pcxi; } ul_arch_ctx_t;

/* IRQ control — no-op stubs for single-threaded host tests */
typedef uint32_t ul_arch_irq_key_t;
static inline ul_arch_irq_key_t ul_arch_cpu_irq_save(void)       { return 0u; }
static inline void ul_arch_cpu_irq_restore(ul_arch_irq_key_t k)  { (void)k; }
static inline void ul_arch_cpu_irq_enable(void)                   { }
static inline void ul_arch_cpu_irq_disable(void)                  { }

/* CLZ — GCC built-in equivalent for host x86 */
static inline uint32_t ul_arch_cpu_clz(uint32_t v)
{
	return v ? (uint32_t)__builtin_clz(v) : 32u;
}

/* MPU region descriptor */
#define UL_ARCH_MAX_REGIONS	12

typedef struct {
	uintptr_t base;
	size_t    size;
	uint32_t  perms;
	uint8_t   type;
} ul_arch_region_t;

#define UL_REGION_CODE		0
#define UL_REGION_DATA		1
#define UL_REGION_STACK		2
#define UL_REGION_HEAP		3
#define UL_REGION_PERIPH	4
#define UL_REGION_SHARED	5

/* Context — forward declarations; test file provides implementations */
void ul_arch_ctx_switch(ul_arch_ctx_t *from, ul_arch_ctx_t *to);
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t stack_top, int priv);
void ul_arch_ctx_free(ul_arch_ctx_t *ctx);
void ul_arch_mpu_switch(const ul_arch_region_t *regions, uint8_t count,
			uint8_t prs);

#endif /* UL_ARCH_H */
