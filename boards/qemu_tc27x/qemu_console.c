/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * QEMU TC27x console — boards/qemu_tc27x/qemu_console.c
 *
 * Provides ul_printk_char_out() and ul_sim_exit() for the Linumiz QEMU-AURIX
 * target (qemu-system-tricore -machine KIT_AURIX_TC277_TRB).
 *
 * Linumiz QEMU-AURIX VIRT debug device (hw/tricore/tricore_virt.c):
 *   0xBF000020  uint32_t  write byte → putchar on QEMU stdout
 *   0xBF000028  uint32_t  write code → exit(code) inside QEMU
 */

#include <stdint.h>
#include <kernel/include/ul_printk.h>

#define VIRT_BASE    0xBF000000UL
#define VIRT_PUTCHAR (*(volatile uint32_t *)(VIRT_BASE + 0x20U))
#define VIRT_EXIT    (*(volatile uint32_t *)(VIRT_BASE + 0x28U))

void ul_printk_char_out(char c)
{
	VIRT_PUTCHAR = (uint32_t)(uint8_t)c;
}

__attribute__((noreturn)) void ul_sim_exit(int code)
{
	VIRT_EXIT = (uint32_t)code;
	for (;;)
		;
}
