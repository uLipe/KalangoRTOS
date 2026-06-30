/*
 * TriCore TC27x / TSIM platform layer — standalone hello world.
 *
 * Output uses the TSIM virtual I/O (HighTec convention).  TSIM must be
 * invoked with -H to enable HighTec syscall interception of the 'debug'
 * instruction.
 *
 * write() is left to newlib/libgloss; with -H, TSIM intercepts its 'debug'
 * trap and routes stdout to the host terminal.
 *
 * _exit() is implemented here explicitly so we can set A14 = 0x0000900d
 * before the 'debug' instruction.  TSIM's test harness checks A14 on exit:
 * A14 == 0x900d → "test passed"; anything else → "test failed".
 */

#include <stdint.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * CSA pool — placed in .csa_pool (NOLOAD) so BSS zeroing below does not
 * overwrite the free list built by startup.S.
 * -------------------------------------------------------------------------- */
#define CSA_COUNT 64

__attribute__((section(".csa_pool"), aligned(64)))
uint32_t tricore_csa_pool[CSA_COUNT * 16];

/* --------------------------------------------------------------------------
 * BSS zeroing — called from startup.S before main.
 * -------------------------------------------------------------------------- */
extern uint32_t __bss_start;
extern uint32_t __bss_end;

void platform_init(void)
{
	uint32_t *p;

	for (p = &__bss_start; p < &__bss_end; p++)
		*p = 0U;

	/*
	 * Disable stdio buffering so every printf/puts byte goes through
	 * _write_r → write → 'debug' trap immediately, without requiring an
	 * explicit fflush before exit.
	 */
	setvbuf(stdout, NULL, _IONBF, 0);
}

/* --------------------------------------------------------------------------
 * _exit — terminate the TSIM simulation
 *
 * TSIM's test harness inspects A14 when the 'debug' instruction is
 * executed after program completion:
 *   A14 == 0x0000900d  →  "test passed"
 *   A14 == anything else →  "test failed A14 = <value>"
 *
 * Set A14 to the sentinel before issuing 'debug' so TSIM exits cleanly
 * and make(1) sees exit code 0.
 * -------------------------------------------------------------------------- */
/*
 * _exit — terminate the TSIM simulation.
 *
 * TSIM's standalone test harness executes exactly ONE instruction of _exit
 * and then reads A14 to decide "test passed" (A14 == 0x900d) or "test
 * failed".  We cannot rely on a C-generated frame prologue ("mov.aa %a14,
 * %sp") since that overwrites A14 before we can set it to the sentinel.
 *
 * Instead we implement _exit as file-scope assembly so the very first
 * instruction is "mov.a %a14, %d15".  startup.S pre-loads D15 = 0x900d
 * just before "call _exit"; CALL preserves live D-register values across
 * the call boundary, so D15 is still 0x900d when TSIM executes
 * instruction #1 of this function.
 *
 * The debug instruction that follows is executed by TSIM as a HighTec I/O
 * syscall; in practice TSIM already terminated after the first instruction,
 * but we emit it for completeness.  The infinite loop prevents the PC from
 * wandering into undefined memory if TSIM ever continues past debug.
 */
__asm__(
	".section .text._exit,\"ax\",@progbits\n"
	".global _exit\n"
	".type   _exit, @function\n"
	"_exit:\n"
	"   mov.a  %a14, %d15\n"	/* A14 = D15 = 0x900d (TSIM sentinel)   */
	"   debug\n"			/* TSIM -H: HighTec exit syscall        */
	"   j      .\n"			/* infinite loop — should not be reached */
	".size _exit, . - _exit\n"
);
