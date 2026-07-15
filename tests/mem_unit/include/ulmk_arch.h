#ifndef ULMK_ARCH_H
#define ULMK_ARCH_H
#include <stdint.h>
typedef uint32_t ulmk_arch_irq_key_t;
typedef struct { volatile uint32_t locked; } ulmk_spinlock_t;
#define ULMK_SPINLOCK_INIT { 0u }
static inline ulmk_arch_irq_key_t ulmk_arch_cpu_irq_save(void) { return 0; }
static inline void ulmk_arch_cpu_irq_restore(ulmk_arch_irq_key_t k) { (void)k; }
static inline void ulmk_arch_spin_lock(ulmk_spinlock_t *l)
{ while (__sync_lock_test_and_set(&l->locked, 1u)) ; }
static inline void ulmk_arch_spin_unlock(ulmk_spinlock_t *l)
{ __sync_lock_release(&l->locked); }
static inline ulmk_arch_irq_key_t ulmk_arch_spin_lock_irqsave(ulmk_spinlock_t *l)
{ ulmk_arch_irq_key_t k = ulmk_arch_cpu_irq_save(); ulmk_arch_spin_lock(l); return k; }
static inline void ulmk_arch_spin_unlock_irqrestore(ulmk_spinlock_t *l, ulmk_arch_irq_key_t k)
{ ulmk_arch_spin_unlock(l); ulmk_arch_cpu_irq_restore(k); }
#endif
