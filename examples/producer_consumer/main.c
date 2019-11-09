#include <stdio.h>

#include <platform_qemu.h>

#include <KalangoRTOS/kalango_api.h>
#include "mock_peripheral.h"

typedef struct {
    uint32_t sample;
} sensor_msg_t;

static QueueId sensor_queue;
static mock_sensor_t sensor;

static void producer_task(void *arg)
{
    sensor_msg_t msg;
    (void)arg;

    for (uint32_t i = 1; i <= 5; i++) {
        mock_sensor_set(&sensor, i * 10);
        msg.sample = mock_sensor_read(&sensor);
        Kalango_QueueInsert(sensor_queue, &msg, sizeof(msg), KERNEL_WAIT_FOREVER);
        Kalango_Sleep(20);
    }
}

static void consumer_task(void *arg)
{
    sensor_msg_t msg;
    uint32_t size = sizeof(msg);
    uint32_t received = 0;
    (void)arg;

    for (;;) {
        if (Kalango_QueueRemove(sensor_queue, &msg, &size, 50) == kSuccess) {
            received++;
            printf("consumer got sample %lu\n", (unsigned long)msg.sample);
            if (received >= 5) {
                printf("producer_consumer: PASS\n");
                platform_exit(0);
            }
        }
    }
}

void Kalango_MainTask(void *arg)
{
    TaskSettings prod = {
        .priority = 3,
        .stack_size = 512,
        .function = producer_task,
        .arg = NULL,
    };
    TaskSettings cons = {
        .priority = 2,
        .stack_size = 512,
        .function = consumer_task,
        .arg = NULL,
    };

    (void)arg;

    mock_sensor_init(&sensor);
    sensor_queue = Kalango_QueueCreate(4, sizeof(sensor_msg_t));

    Kalango_TaskCreate(&cons);
    Kalango_TaskCreate(&prod);

    printf("producer_consumer: starting\n");
}

int main(void)
{
    Kalango_CoreStart();
    platform_exit(1);

    return 0;
}
