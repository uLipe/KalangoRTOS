/* stub ul_arch.h for host unit tests */
#ifndef UL_ARCH_H
#define UL_ARCH_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uintptr_t pc; uintptr_t sp; } ul_arch_ctx_t;
typedef uint32_t ul_arch_irq_key_t;

#define UL_ARCH_MAX_REGIONS 12
typedef struct {
	uintptr_t base;
	size_t    size;
	uint32_t  perms;
	uint8_t   type;
} ul_arch_region_t;

static inline void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
				    void (*entry)(void *), void *arg,
				    uintptr_t sp, int priv)
{
	(void)priv;
	ctx->pc = (uintptr_t)entry;
	ctx->sp = sp;
	(void)arg;
}

static inline void ul_arch_ctx_switch(ul_arch_ctx_t *from,
				      ul_arch_ctx_t *to)
{
	(void)from; (void)to;
}

static inline uint32_t ul_arch_tick_get(void) { return 0; }
static inline void ul_arch_tick_deadline(uint32_t t) { (void)t; }
static inline ul_arch_irq_key_t ul_arch_cpu_irq_save(void) { return 0; }
static inline void ul_arch_cpu_irq_restore(ul_arch_irq_key_t k) { (void)k; }
#endif
