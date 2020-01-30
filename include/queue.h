#pragma once

#include <kernel_types.h>
#include <core.h>
#include <sched.h>
#include <clock.h>
#include <object_pool.h>

QueueId QueueCreate(uint32_t noof_slots, uint32_t slot_size);
KernelResult QueueInsert(QueueId queue, void *data, uint32_t data_size, uint32_t timeout);
KernelResult QueuePeek(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout);
KernelResult QueueRemove(QueueId queue, void *data, uint32_t *data_size, uint32_t timeout);
KernelResult QueueDelete(QueueId queue);

