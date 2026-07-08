/* SPDX-License-Identifier: MIT */
/* boards/qemu_mps2_an500/board_console.h */

#ifndef BOARD_CONSOLE_H
#define BOARD_CONSOLE_H

#include <ulmk/microkernel.h>
#include "board_config.h"

ulmk_tid_t board_console_start(const ulmk_boot_info_t *info);
void       board_console_putc(char c);
void       board_console_puts(const char *s);

#endif /* BOARD_CONSOLE_H */
