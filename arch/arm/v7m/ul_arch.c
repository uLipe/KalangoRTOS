/*
 * MIT License
 *
 * Copyright (c) 2024 Felipe Neves
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <ulipeMicroKernel/ul_microkernel.h>

#define ARCH_NUM_EXCEPTIONS 16
#define ARCH_NUM_IRQS       240

struct arch_interrupt_descriptor {
    void (*fn) (void *arg);
    void *user_data;
};

static __attribute__((aligned(16))) uint32_t base_msp[128];
uint32_t *estack = &base_msp[0] + sizeof(base_msp);

static struct arch_interrupt_descriptor interrupt_table[ARCH_NUM_IRQS + ARCH_NUM_EXCEPTIONS] = {
    {NULL,NULL},
};

void ul_arch_isr_entry(uint32_t regs, uint32_t intno)
{

}

int ul_arch_count_lead_zeros(uint32_t val)
{
    return 0;
}

int ul_arch_init(void)
{
    return 0;
}

int ul_arch_kmicrokernel_start(void);
{
    return 0;
}

int ul_arch_irq_lock(void);
{
    return 0;
}

int ul_arch_enable_irq(int irq_number)
{
    return 0;
}

int ul_arch_disable_irq(int irq_number)
{
    return 0;
}

int ul_arch_irq_unlock(int key);
{
    return 0;
}

int ul_arch_get_next_pending_isr(void)
{
    return 0;
}

int ul_arch_dispatch_isr(int irq_number)
{
    return 0;
}

int ul_arch_install_isr(int irq_number, void (*fn) (void *user_data), void *user_data);
{
    return 0;
}

void ul_arch_new_thread(struct ul_thread *th)
{
}

int ul_arch_switch(struct ul_thread *from, struct ul_thread *to)
{
    return 0;
}

struct ul_thread * ul_arch_current_thread(void)
{
    return NULL;
}

int ul_arch_idle(void)
{

}