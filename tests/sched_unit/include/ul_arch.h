/* Stub for unit tests — provides just enough to compile sched + fifo_rt */
#ifndef UL_ARCH_H
#define UL_ARCH_H

#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t pcxi; } ul_arch_ctx_t;

void ul_arch_ctx_switch(ul_arch_ctx_t *from, ul_arch_ctx_t *to);
void ul_arch_ctx_init(ul_arch_ctx_t *ctx,
		      void (*entry)(void *), void *arg,
		      uintptr_t stack_top, int priv);

#endif /* UL_ARCH_H */
