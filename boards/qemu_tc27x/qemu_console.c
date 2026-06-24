/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * QEMU TC27x console — boards/qemu_tc27x/qemu_console.c
 *
 * Provides the strong definition of ul_printk_char_out() for the
 * Linumiz QEMU-AURIX target (qemu-system-tricore -machine KIT_AURIX_TC277_TRB).
 *
 * Linumiz QEMU-AURIX VIRT debug device (hw/tricore/tricore_virt.c):
 *   0xBF000020  uint32_t  write byte → putchar on QEMU stdout; write 0 → fflush
 *   0xBF000028  uint32_t  write code → exit(code) inside QEMU
 *
 * This device does not exist on real TC27x silicon.
 */

#include <stdint.h>
#include <kernel/include/ul_printk.h>

#define VIRT_BASE    0xBF000000UL
#define VIRT_PUTCHAR (*(volatile uint32_t *)(VIRT_BASE + 0x20U))

void ul_printk_char_out(char c)
{
	VIRT_PUTCHAR = (uint32_t)(uint8_t)c;
}
