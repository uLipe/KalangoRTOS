#include <kalango_api.h>
#include <kernel_samples.h>
#include <nrfx.h>
#include <printf.h>
#include <SEGGER_RTT.h>

#define LED1_PIN 0x02
#define LED2_PIN 0x03
#define LED3_PIN 0x04

const char * const colors[5] = {
    NULL,
    NULL,
    RTT_CTRL_TEXT_BRIGHT_RED,
    RTT_CTRL_TEXT_BRIGHT_GREEN,
    RTT_CTRL_TEXT_BRIGHT_CYAN,
};


static void BlinkTask(uint32_t led_arg) {
 
    NRF_P0->PIN_CNF[led_arg] =
          (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos)
        | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_PULL_Pos);

    for(;;) {
        Kalango_Sleep(200);
        printf("%s Ticks: %d ::: Heap free: %d ::: Kalango thread 0x%p is blinking led: %d \n\n",
                colors[led_arg], Kalango_GetCurrentTicks(), GetKernelFreeBytesOnHeap(), Kalango_GetCurrentTaskId(), led_arg);
        NRF_P0->OUT ^= 1UL << led_arg;;
    }
}

int main(void) {

    TaskSettings settings;

    settings.arg = (void *)LED1_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 8;
    settings.stack_size = 2048;

    Kalango_TaskCreate(&settings);

    settings.arg = (void *)LED2_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 4;
    settings.stack_size = 2048;

    Kalango_TaskCreate(&settings);

    settings.arg = (void *)LED3_PIN;
    settings.function = (TaskFunction)BlinkTask;
    settings.priority = 7;
    settings.stack_size = 2048;

    Kalango_TaskCreate(&settings);

    Kalango_CoreStart();
}