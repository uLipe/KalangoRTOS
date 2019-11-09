#pragma once

#include <stdint.h>

void platform_init(void);
void platform_putchar(char c);
void platform_exit(int code) __attribute__((noreturn));
void platform_trigger_test_irq(void);
