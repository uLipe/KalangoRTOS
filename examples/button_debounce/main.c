#include <stdio.h>

#include <platform_qemu.h>

#include <KalangoRTOS/kalango_api.h>
#include "mock_peripheral.h"

static mock_button_t button;
static volatile uint32_t debounced_count;

static void debounce_timer_cb(void *user)
{
    mock_button_t *btn = (mock_button_t *)user;

    if (btn->pressed) {
        debounced_count++;
        printf("debounced press count %lu\n", (unsigned long)debounced_count);
        if (debounced_count >= 3) {
            printf("button_debounce: PASS\n");
            platform_exit(0);
        }
    }
}

static void button_sim_task(void *arg)
{
    (void)arg;

    for (uint32_t i = 0; i < 3; i++) {
        Kalango_Sleep(30);
        mock_button_press(&button);
        //Hold longer than the debounce window (10 ticks) plus the poll period so
        //the press-triggered timer samples while the button is still down.
        Kalango_Sleep(20);
        mock_button_release(&button);
        Kalango_Sleep(30);
    }
}

static void button_poll_task(void *arg)
{
    TimerId debounce_timer = Kalango_TimerCreate(debounce_timer_cb, 10, 0, arg);
    (void)arg;

    Kalango_TimerStart(debounce_timer);

    for (;;) {
        if (mock_button_consume_event(&button)) {
            Kalango_TimerStop(debounce_timer);
            Kalango_TimerSetValues(debounce_timer, 10, 0);
            Kalango_TimerStart(debounce_timer);
        }
        Kalango_Sleep(2);
    }
}

void Kalango_MainTask(void *arg)
{
    TaskSettings sim = {
        .priority = 2,
        .stack_size = 512,
        .function = button_sim_task,
        .arg = NULL,
    };
    TaskSettings poll = {
        .priority = 3,
        .stack_size = 512,
        .function = button_poll_task,
        .arg = &button,
    };

    (void)arg;

    mock_button_init(&button);
    debounced_count = 0;

    Kalango_TaskCreate(&poll);
    Kalango_TaskCreate(&sim);

    printf("button_debounce: starting\n");
}

int main(void)
{
    Kalango_CoreStart();
    platform_exit(1);

    return 0;
}
