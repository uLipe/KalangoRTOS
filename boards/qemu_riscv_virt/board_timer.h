/* SPDX-License-Identifier: MIT */
/* boards/qemu_riscv_virt/board_timer.h */

#ifndef BOARD_TIMER_H
#define BOARD_TIMER_H

#include <stdint.h>
#include <ulmk/microkernel.h>

#define BOARD_TIMER_RTC_BASE		0x00101000u
#define BOARD_TIMER_RTC_MAP_SIZE	0x1000u
#define BOARD_TIMER_PLIC_IRQ		11u
#define BOARD_TIMER_HW_CLOCK_HZ		1000000000u

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info);
void       board_timer_sleep_us(uint32_t us);

#endif /* BOARD_TIMER_H */
