#pragma once

#include <stdint.h>
#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/kalango_config_internal.h>

#if CONFIG_ENABLE_SOFTIRQ > 0

typedef void (*SoftIrqHandler)(void *data);

/**
 * @brief Initialise the softirq subsystem and spawn the kSoftIrqD daemon.
 *
 * Called once from CoreStart(). Creates the internal semaphore and the
 * kSoftIrqD high-priority task that processes pending handler invocations.
 */
KernelResult SoftIrqInit(void);

/**
 * @brief Install a handler for the given softirq vector.
 *
 * Safe to call from task context at any time.
 *
 * @param vector  Softirq vector index (0 .. CONFIG_SOFTIRQ_MAX_VECTORS-1).
 * @param handler Function to call when the vector fires, or NULL to detach.
 */
KernelResult SoftIrqRequest(uint8_t vector, SoftIrqHandler handler);

/**
 * @brief Pend a softirq vector with the given data pointer.
 *
 * Called directly from the SVC #1 handler in arch assembly.  May also be
 * called from C (task or ISR context) when the SVC path is not desirable.
 *
 * @param vector  Softirq vector index.
 * @param data    Opaque pointer forwarded to the installed handler.
 */
void SoftIrqPend(uint8_t vector, void *data);

#endif /* CONFIG_ENABLE_SOFTIRQ */
