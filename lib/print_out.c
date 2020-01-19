#include "SEGGER_RTT.h"

void _putchar(char c) {
    SEGGER_RTT_Write(0, &c, 1);
}

