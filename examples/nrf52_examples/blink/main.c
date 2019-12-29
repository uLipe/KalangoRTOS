#include <kalango_api.h>
#include <kernel_samples.h>
#include <nrf.h>

#define LED1_PIN 0x02
#define LED2_PIN 0x03
#define LED3_PIN 0x04

static uint8_t blink1_stack[256];
static uint8_t blink2_stack[256];
static uint8_t blink3_stack[256];

static void BlinkTask(uint32_t led_arg) {
 
    NRF_P0->PIN_CNF[led_arg] =
          (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
        | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_PULL_Pos);

    for(;;) {
        Kalango_Sleep(led_arg);
        NRF_P0->OUT ^= 1UL << led_arg;;
    }
}

int main(void) {

    TaskSettings settings;

    settings.arg = (void *)LED1_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 8;
    settings.stack_area = blink1_stack;
    settings.stack_size = 256;

    Kalango_TaskCreate(&settings);

    settings.arg = (void *)LED2_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 4;
    settings.stack_area = blink2_stack;
    settings.stack_size = 256;

    Kalango_TaskCreate(&settings);

    settings.arg = (void *)LED3_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 7;
    settings.stack_area = blink3_stack;
    settings.stack_size = 256;

    Kalango_TaskCreate(&settings);

    Kalango_CoreStart();
}