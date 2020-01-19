#include "kernel_samples.h"

static TaskId task_a; 
static TaskId task_b; 

static void DemoTask1(void *arg) {
    uint32_t noof_wakeups = 0;
    
    for(;;) {
        Kalango_Sleep(250);
        noof_wakeups++;
    }
}

static void DemoTask2(void *arg) {
    uint32_t noof_wakeups = 0;

    for(;;) {
        Kalango_Sleep(25);
        noof_wakeups++;
    }
}

int TaskSample (void) {
    TaskSettings settings;

    settings.arg = NULL;
    settings.function = DemoTask1;
    settings.priority = 8;
    settings.stack_size = 512;

    task_a = Kalango_TaskCreate(&settings);
    
    settings.arg = NULL;
    settings.function = DemoTask2;
    settings.priority = 4;
    settings.stack_size = 512;

    task_b = Kalango_TaskCreate(&settings);

    (void)task_a;
    (void)task_b;

    Kalango_CoreStart();
    return 0;
}