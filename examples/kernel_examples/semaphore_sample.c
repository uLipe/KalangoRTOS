#include "kernel_samples.h"

static SemaphoreId sync;
static uint32_t noof_gives = 0;
static uint32_t noof_syncs = 0;

static void DemoTask1(void *arg) {
    sync = Kalango_SemaphoreCreate(0, 1);
    for(;;) {
        Kalango_SemaphoreTake(sync, KERNEL_WAIT_FOREVER);
        noof_syncs++;
    }
}

static void DemoTask3(void *arg) {
    for(;;) {
        Kalango_SemaphoreGive(sync, 1);
        noof_gives++;
    }
}

int SemaphoreSample (void) {
    TaskSettings settings;

    settings.arg = NULL;
    settings.function = DemoTask1;
    settings.priority = 8;
    settings.stack_size = 256;

    TaskId task_a = Kalango_TaskCreate(&settings);
    (void)task_a;

    settings.arg = NULL;
    settings.function = DemoTask3;
    settings.priority = 2;
    settings.stack_size = 256;

    TaskId task_c = Kalango_TaskCreate(&settings);
    (void)task_c;

    Kalango_CoreStart();
    return 0;
}