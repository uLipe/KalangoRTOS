#include <KalangoRTOS/softirq.h>
#include <KalangoRTOS/kalango_config_internal.h>

#if CONFIG_ENABLE_SOFTIRQ > 0

#include <KalangoRTOS/kalango_api.h>
#include <KalangoRTOS/arch.h>

static SoftIrqHandler softirq_table[CONFIG_SOFTIRQ_MAX_VECTORS];
static void          *softirq_data[CONFIG_SOFTIRQ_MAX_VECTORS];
static volatile uint32_t softirq_pending;
static SemaphoreId    softirq_sem;

/*
 * kSoftIrqD — high-priority daemon that drains the pending softirq bitmap.
 *
 * Runs at CONFIG_PRIORITY_LEVELS-2 (just below the main task) so it
 * preempts normal application tasks but does not starve the kernel's own
 * main task.  A single semaphore give per SoftIrqPend call is sufficient
 * as a wakeup signal; the daemon always drains the full bitmap before
 * waiting again.
 */
static void SwIRQHandler(void)
{
    Kalango_SemaphoreGive(softirq_sem, 1);
}

static void kSoftIrqD(void *arg)
{
    (void)arg;

    for (;;) {
        Kalango_SemaphoreTake(softirq_sem, KERNEL_WAIT_FOREVER);

        ArchCriticalSectionEnter();
        uint32_t pending = softirq_pending;
        softirq_pending  = 0U;
        ArchCriticalSectionExit();

        while (pending) {
            uint32_t bit   = (uint32_t)__builtin_ctz(pending);
            pending       &= pending - 1U;
            SoftIrqHandler h    = softirq_table[bit];
            void          *data = softirq_data[bit];
            if (h != NULL) {
                h(data);
            }
        }
    }
}

void SoftIrqPend(uint8_t vector, void *data)
{
    if (vector >= CONFIG_SOFTIRQ_MAX_VECTORS) {
        return;
    }

    ArchCriticalSectionEnter();
    softirq_data[vector]  = data;
    softirq_pending      |= (1U << vector);
    ArchCriticalSectionExit();

    ArchSwIrqPend();
}

KernelResult SoftIrqRequest(uint8_t vector, SoftIrqHandler handler)
{
    if (vector >= CONFIG_SOFTIRQ_MAX_VECTORS) {
        return kErrorInvalidParam;
    }

    ArchCriticalSectionEnter();
    softirq_table[vector] = handler;
    ArchCriticalSectionExit();

    return kSuccess;
}

KernelResult SoftIrqInit(void)
{
    softirq_sem = Kalango_SemaphoreCreate(0, CONFIG_SOFTIRQ_MAX_VECTORS);
    if (softirq_sem == NULL) {
        return kErrorNotEnoughKernelMemory;
    }

    ArchSwIrqBind(SwIRQHandler);

    TaskSettings s = {
        .priority   = CONFIG_PRIORITY_LEVELS - 2U,
        .stack_size = CONFIG_SOFTIRQ_TASK_STACK_SIZE,
        .function   = kSoftIrqD,
        .arg        = NULL,
    };

    return (Kalango_TaskCreate(&s) != NULL) ? kSuccess : kErrorNotEnoughKernelMemory;
}

#endif /* CONFIG_ENABLE_SOFTIRQ */
