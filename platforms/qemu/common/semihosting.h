#pragma once

#include <stdint.h>

enum {
    SEMIHOST_SYS_EXIT = 0x18,
    SEMIHOST_SYS_EXIT_EXTENDED = 0x20,
    SEMIHOST_SYS_WRITEC = 0x03,
    SEMIHOST_SYS_WRITE0 = 0x04,
};

int32_t semihost_call(uint32_t reason, void *arg);
