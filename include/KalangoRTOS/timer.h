#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/sched.h>
#include <KalangoRTOS/clock.h>
#include <KalangoRTOS/object_pool.h>

TimerId TimerCreate(TimerCallback callback, uint32_t expiry_time, uint32_t period_time, void *user_data);
KernelResult TimerStart(TimerId timer);
KernelResult TimerStop(TimerId timer);
KernelResult TimerSetValues(TimerId timer, uint32_t expiry_time, uint32_t period_time);
KernelResult TimerDelete(TimerId timer);


