/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * TSIM TC27x console — boards/tsim_tc27x/tsim_console.c
 *
 * Provides ul_printk_char_out(), ul_sim_exit(), and _exit() for the
 * Infineon TSIM instruction-set simulator (tsim16p_e).
 *
 * TSIM Virtual I/O (VIO) uses the HighTec debug-trap protocol (-H flag):
 *   - debug instruction with D12 = syscall number
 *   - write(2, buf, len) => D12=5, D4=fd, D5=buf, D6=len
 *
 * TSIM standalone test mode (-s flag) monitors the FIRST instruction
 * of _exit: if A14 == 0x900d the test passes; any other value fails.
 * startup.S pre-loads D15 = 0x900d before "call _exit"; this file's
 * _exit implementation moves D15 → A14 as its very first instruction
 * so that TSIM records a pass.  For failure paths D15 is left at 0.
 */

#include <stdint.h>
#include <kernel/include/ul_printk.h>

/* =========================================================================
 * ul_printk_char_out — single-character output via TSIM HighTec VIO
 *
 * Delegates to newlib's write() (linked via -lc) which calls the internal
 * ___virtio shim: sets D12=5, then 'debug'.  This is the same path used by
 * printf() in the hello-world example and is the only path verified to
 * produce visible output under TSIM's -H semihosting mode.
 * ========================================================================= */

extern int write(int fd, const void *buf, unsigned int len);

void ul_printk_char_out(char c)
{
	write(1, &c, 1);
}

/* =========================================================================
 * ul_sim_exit — terminate the simulation
 *
 * For pass (code == 0): pre-loads D15 = 0x900d so that _exit's first
 * instruction (mov.a %a14, %d15) sets A14 = 0x900d → TSIM "test passed".
 * For fail (code != 0): D15 = 0 → A14 = 0 → TSIM "test failed".
 * ========================================================================= */

__attribute__((noreturn)) void ul_sim_exit(int code)
{

	/*
	 * Use a direct tail-jump to _exit (no CALL, no new CSA frame) so D15
	 * is not clobbered between the assignment and _exit's first instruction
	 * "mov.a %a14, %d15".  TSIM standalone (-s) checks A14 when the debug
	 * instruction in _exit executes: A14 == 0x900d → "test passed".
	 */
	if (code == 0) {
		__asm__ volatile(
			"mov.u  %%d15, 0x900d\n\t"
			"j      _exit"
			::: "d15");
	} else {
		__asm__ volatile(
			"mov    %%d15, 0\n\t"
			"j      _exit"
			::: "d15");
	}
	__builtin_unreachable();
}

/* =========================================================================
 * _exit — TSIM simulation termination point
 *
 * TSIM standalone (-s) mode intercepts the FIRST instruction of _exit and
 * reads A14 to determine pass/fail.  This function is implemented entirely
 * in file-scope assembly to guarantee the first instruction is the A14 load.
 *
 * Calling convention: ul_sim_exit() loads D15 = 0x900d before the CALL;
 * CALL preserves D15, so it is still valid on entry here.
 * ========================================================================= */
__asm__(
	".section .text._exit,\"ax\",@progbits\n"
	".global _exit\n"
	".type   _exit, @function\n"
	"_exit:\n"
	"	mov.a  %a14, %d15\n"	/* A14 = 0x900d → TSIM pass */
	"	debug\n"		/* HighTec semihosting exit */
	"	j      .\n"		/* spin — should not be reached */
	".size _exit, . - _exit\n"
);
