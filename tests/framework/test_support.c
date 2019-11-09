#include "unity.h"
#include <KalangoRTOS/kalango_api.h>
#include <platform_qemu.h>

static int teardown_count = 0;

void setUp(void)
{
}

void tearDown(void)
{
    teardown_count++;
    platform_putchar('0' + (char)(teardown_count % 10));
    Kalango_Sleep(10);
    platform_putchar('.');
}
