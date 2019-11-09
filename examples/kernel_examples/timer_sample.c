#include "kernel_samples.h"

static uint8_t stack_1[256];

struct UserData {
    SemaphoreId sync;
    TimerId timer;
};

static struct UserData user_data;

static void OnTimerExpire(void *arg) {
    struct UserData *data = CONTAINER_OF(arg, struct UserData, timer);
    Kalango_SemaphoreGive(data->sync, 1);
    (void)data->timer;
}

static void DemoTask1(void *arg) {
    uint32_t ticks = 0;
    user_data.sync = Kalango_SemaphoreCreate(0, 1);
    user_data.timer = Kalango_TimerCreate(OnTimerExpire, 500, 500);
    
    Kalango_TimerStart(user_data.timer);
    for(;;) {
        Kalango_SemaphoreTake(user_data.sync, KERNEL_WAIT_FOREVER);
        ticks++;
    }
}


int TimerSample (void) {
    TaskSettings settings;

    settings.arg = NULL;
    settings.function = DemoTask1;
    settings.priority = 8;
    settings.stack_area = stack_1;
    settings.stack_size = 256;

    TaskId task_a = Kalango_TaskCreate(&settings);
    (void)task_a;
    Kalango_CoreStart();
    
    return 0;
}