#include <platform_qemu.h>

typedef struct {
    volatile uint32_t ISER[8];
    uint32_t reserved0[24];
    volatile uint32_t ICER[8];
    uint32_t reserved1[24];
    volatile uint32_t ISPR[8];
} nvic_t;

#define NVIC_BASE   0xE000E100UL
#define NVIC        ((nvic_t *)NVIC_BASE)

void platform_trigger_test_irq(void)
{
    NVIC->ISER[0] = 1U;
    NVIC->ISPR[0] = 1U;
}
