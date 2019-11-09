#include "kernel_samples.h"

static uint8_t stack_1[256];
static uint8_t stack_2[256];
static MutexId mutex;

static char console_buffer[128];

static void DemoTask1(void *arg) {
    uint32_t noof_hits = 0;
    mutex = MutexCreate();

    for(;;) {
        Sleep(5);
        MutexLock(mutex, KERNEL_WAIT_FOREVER);
        strcpy(console_buffer, "This is a short message!!");

        if(!strcmp(console_buffer,"This is a short message!!")) {
            noof_hits++;
        }
        
        MutexUnlock(mutex);
    }
}

static void DemoTask2(void *arg) {
    uint32_t noof_hits = 0;

    for(;;) {
        MutexLock(mutex, KERNEL_WAIT_FOREVER);
        strcpy(console_buffer, "The quick brown fox jumps over the lazy dog The quick brown fox jumps over the lazy dog");
        if(!strcmp(console_buffer,"The quick brown fox jumps over the lazy dog The quick brown fox jumps over the lazy dog")) {
            noof_hits++;
        }
        MutexUnlock(mutex);
    }
}

int MutexSample (void) {
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
    
    Kalango_CoreStart();
    return 0;
}