/* stub ulmk_arch.h for host unit tests */
#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef ULMK_ARCH_NUM_CPU
#define ULMK_ARCH_NUM_CPU	1
#endif

typedef struct { uintptr_t pc; uintptr_t sp; } ulmk_arch_ctx_t;
typedef uint32_t ulmk_arch_irq_key_t;

typedef struct { volatile uint32_t locked; } ulmk_spinlock_t;
#define ULMK_SPINLOCK_INIT	{ 0u }

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
static inline uint32_t ulmk_arch_cpu_id(void) { return 0u; }
static inline void ulmk_arch_spin_lock(ulmk_spinlock_t *l)
{ while (__sync_lock_test_and_set(&l->locked, 1u)) ; }
static inline void ulmk_arch_spin_unlock(ulmk_spinlock_t *l)
{ __sync_lock_release(&l->locked); }
static inline ulmk_arch_irq_key_t ulmk_arch_spin_lock_irqsave(ulmk_spinlock_t *l)
{ ulmk_arch_irq_key_t k = ulmk_arch_cpu_irq_save(); ulmk_arch_spin_lock(l); return k; }
static inline void ulmk_arch_spin_unlock_irqrestore(ulmk_spinlock_t *l,
						    ulmk_arch_irq_key_t k)
{ ulmk_arch_spin_unlock(l); ulmk_arch_cpu_irq_restore(k); }
static inline void ulmk_arch_send_ipi(uint32_t c) { (void)c; }
static inline uint32_t ulmk_arch_cpu_clz(uint32_t v)
{ return v ? (uint32_t)__builtin_clz(v) : 32u; }
#endif
