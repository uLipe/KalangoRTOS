#pragma once

#include <KalangoRTOS/kernel_types.h>
#include <KalangoRTOS/core.h>
#include <KalangoRTOS/task.h>

uint32_t GetTicksPerSecond();
uint32_t GetCurrentTicks();
KernelResult Sleep(uint32_t ticks);
KernelResult ClockStep(uint32_t ticks);
KernelResult AddTimeout(Timeout *timeout,
                    uint32_t value,
                    TimeoutCallback timeout_callback);
KernelResult RemoveTimeout(Timeout *timeout);
