#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/object_pool.h>

QueueId QueueCreate(uint32_t noof_slots, uint32_t slot_size);
KernelResult QueueInsert(QueueId queue, void *data, uint32_t data_size, uint32_t timeout);
KernelResult QueuePeek(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout);
KernelResult QueueRemove(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout);
KernelResult QueueDelete(QueueId queue);

