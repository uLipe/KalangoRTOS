/* SPDX-License-Identifier: MIT */
/* boards/qemu_mps2_an500/board_timer.h */

#ifndef BOARD_TIMER_H
#define BOARD_TIMER_H

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "board_config.h"

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info);
void       board_timer_sleep_us(uint32_t us);

#endif /* BOARD_TIMER_H */
