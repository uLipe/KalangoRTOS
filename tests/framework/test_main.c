#include <platform_qemu.h>

#include <KalangoRTOS/kalango_api.h>
#include "unity.h"
#include "test_suites.h"

void Kalango_MainTask(void *arg)
{
    int failures;

    (void)arg;

    UNITY_BEGIN();
    RunAllTests();
    failures = UNITY_END();

    if (failures == 0) {
        platform_exit(0);
    } else {
        platform_exit(1);
    }
}

int main(void)
{
    Kalango_CoreStart();
    platform_exit(100);

    return 0;
}
