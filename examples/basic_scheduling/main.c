#include <stdio.h>

#include <platform_qemu.h>

#include <KalangoRTOS/kalango_api.h>

static TaskId task_a;
static TaskId task_b;

static void demo_task_fast(void *arg)
{
    uint32_t wakeups = 0;
    (void)arg;

    for (;;) {
        Kalango_Sleep(25);
        wakeups++;
        if (wakeups >= 5) {
            printf("demo_fast done (%lu wakeups)\n", (unsigned long)wakeups);
            platform_exit(0);
        }
    }
}

static void demo_task_slow(void *arg)
{
    uint32_t wakeups = 0;
    (void)arg;

    for (;;) {
        Kalango_Sleep(250);
        wakeups++;
        printf("demo_slow wakeup %lu\n", (unsigned long)wakeups);
    }
}

void Kalango_MainTask(void *arg)
{
    TaskSettings settings;

    (void)arg;

    settings.arg = NULL;
    settings.function = demo_task_slow;
    settings.priority = 4;
    settings.stack_size = 512;
    task_a = Kalango_TaskCreate(&settings);

    settings.function = demo_task_fast;
    settings.priority = 8;
    settings.stack_size = 512;
    task_b = Kalango_TaskCreate(&settings);

    (void)task_a;
    (void)task_b;

    printf("basic_scheduling: starting\n");
}

int main(void)
{
    Kalango_CoreStart();
    platform_exit(1);

    return 0;
}
