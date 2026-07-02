/* stub ulmk_arch.h for host unit tests */
#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uintptr_t pc; uintptr_t sp; } ulmk_arch_ctx_t;
typedef uint32_t ulmk_arch_irq_key_t;

#define ULMK_ARCH_MAX_REGIONS 12
typedef struct {
	uintptr_t base;
	size_t    size;
	uint32_t  perms;
	uint8_t   type;
} ulmk_arch_region_t;

static inline void ulmk_arch_ctx_init(ulmk_arch_ctx_t *ctx,
				    void (*entry)(void *), void *arg,
				    uintptr_t sp, int priv)
{
	(void)priv;
	ctx->pc = (uintptr_t)entry;
	ctx->sp = sp;
	(void)arg;
}

static inline void ulmk_arch_ctx_switch(ulmk_arch_ctx_t *from,
				      ulmk_arch_ctx_t *to)
{
	(void)from; (void)to;
}

static inline uint32_t ulmk_arch_tick_get(void) { return 0; }
static inline void ulmk_arch_tick_deadline(uint32_t t) { (void)t; }
static inline ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void) { return 0; }
static inline void ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t k) { (void)k; }
#endif
