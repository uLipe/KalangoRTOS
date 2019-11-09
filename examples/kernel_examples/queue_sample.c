#include "kernel_samples.h"

#define MESSAGE_STRING      "command\0"
#define MESSAGE_SIZE        sizeof(MESSAGE_STRING)
#define QUEUE_NOOF_SLOTS    1     

static uint8_t stack_1[256];
static uint8_t stack_2[256];
static uint8_t queue_buffer[QUEUE_NOOF_SLOTS * MESSAGE_SIZE];
QueueId queue;

static void DemoTask1(void *arg) {
    char command[16];
    uint32_t slotsize = 0;
    uint32_t command_hits = 0;

    queue = Kalango_QueueCreate(QUEUE_NOOF_SLOTS, MESSAGE_SIZE, queue_buffer);

    for(;;) {
        Kalango_QueuePeek(queue, &command, &slotsize, KERNEL_WAIT_FOREVER);
        if(!strcmp(MESSAGE_STRING, command)) {
            command_hits++;
        }

        Kalango_QueueRemove(queue, &command, &slotsize, 0);
    }
}

static void DemoTask2(void *arg) {
    char command[16];
    strcpy(command, MESSAGE_STRING);

    for(;;) {
        Kalango_QueueInsert(queue, &command, MESSAGE_SIZE, 0);
    }
}

int QueueSample (void) {
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