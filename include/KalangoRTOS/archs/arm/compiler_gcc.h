#pragma once

#define KALANGO_NAKED           __attribute__((naked))
#define KALANGO_USED            __attribute__((used))
#define KALANGO_SECTION(name)   __attribute__((section(name)))
#define KALANGO_ALIGN(n)        __attribute__((aligned(n)))
#define KALANGO_WEAK            __attribute__((weak))
