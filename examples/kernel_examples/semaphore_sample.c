#include "kernel_samples.h"

static uint8_t stack_1[256];
static uint8_t stack_2[256];
static uint8_t stack_3[256];
static SemaphoreId sync;

static void DemoTask1(void *arg) {
    uint32_t noof_syncs = 0;
    sync = Kalango_SemaphoreCreate(0, 1);
    for(;;) {
        Kalango_SemaphoreTake(sync, KERNEL_WAIT_FOREVER);
        noof_syncs++;
    }
}

static void DemoTask2(void *arg) {
    uint32_t noof_syncs;
    
    for(;;) {
        Kalango_SemaphoreTake(sync, KERNEL_WAIT_FOREVER);
        noof_syncs++;
    }
}

static void DemoTask3(void *arg) {
    uint32_t noof_gives = 0;

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
    settings.stack_area = stack_1;
    settings.stack_size = 256;

    TaskId task_a = Kalango_TaskCreate(&settings);
    (void)task_a;

    settings.arg = NULL;
    settings.function = DemoTask2;
    settings.priority = 4;
    settings.stack_area = stack_2;
    settings.stack_size = 256;

    TaskId task_b = Kalango_TaskCreate(&settings);
    (void)task_b;

    settings.arg = NULL;
    settings.function = DemoTask3;
    settings.priority = 2;
    settings.stack_area = stack_3;
    settings.stack_size = 256;

    TaskId task_c = Kalango_TaskCreate(&settings);
    (void)task_c;

    Kalango_CoreStart();
    return 0;
}