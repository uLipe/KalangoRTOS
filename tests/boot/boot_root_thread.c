/* SPDX-License-Identifier: MIT */
/*
 * Boot integration test — tests/boot/boot_root_thread.c
 *
 * Provides ul_root_thread() for the boot integration test only.
 * Prints a sentinel and exits QEMU cleanly so the Makefile can grep
 * for expected output markers.
 *
 * NOT a production implementation. The real root thread is provided
 * by the application layer (boot_model.mdc).
 */

#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>

/* QEMU VIRT device exit register (hw/tricore/tricore_virt.c +0x28). */
#define VIRT_EXIT  (*(volatile unsigned int *)0xBF000028U)

void ul_root_thread(const ul_boot_info_t *info)
{
	(void)info;
	ul_printk("boot_test: root thread reached — BOOT OK\n");
	VIRT_EXIT = 0U;
	for (;;)
		;
}
