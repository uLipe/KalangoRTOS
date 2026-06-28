/* Stub for sleep unit tests — ul_arch.h */
#ifndef UL_ARCH_H
#define UL_ARCH_H

#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t pcxi; } ul_arch_ctx_t;

/* Context switch — stubbed; not exercised by timer unit tests. */
void ul_arch_ctx_switch(ul_arch_ctx_t *from, ul_arch_ctx_t *to);
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t stack_top, int priv);

/* Timer primitives — implementations provided in sleep_unit_test.c. */
uint32_t ul_arch_tick_get(void);
void     ul_arch_tick_deadline(uint32_t delta_us);

#endif /* UL_ARCH_H */
