#include "semihosting.h"

int32_t semihost_call(uint32_t reason, void *arg)
{
    register uint32_t r0 asm("r0") = reason;
    register void *r1 asm("r1") = arg;

    asm volatile (
        "bkpt 0xAB"
        : "+r" (r0)
        : "r" (r1)
        : "memory"
    );

    return (int32_t)r0;
}
