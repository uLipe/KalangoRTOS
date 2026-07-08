/* SPDX-License-Identifier: MIT */
/*
 * boards/qemu_mps2_an505/board_init.c — early hardware bring-up.
 *
 * ulmk_board_init() runs privileged at boot (from ulmk_kern_start, before .data
 * copy and before any driver thread).  It is kept separate from the board
 * service threads (board_services.c) so it is linked even by integration tests
 * that skip the board servers (ARCH_SKIP_BOARD_SVC=1): the peripheral-privilege
 * setup below is mandatory for any unprivileged peripheral access, not just for
 * the console/timer servers.
 */

#include <stdint.h>

/*
 * IoTKit Secure Privilege Control block (SPCTRL, Secure alias 0x50080000).
 * The SSE-200 gates every PPC-fronted peripheral behind a privilege check that
 * defaults to "privileged only"; unprivileged (driver) accesses are silently
 * RAZ/WI'd — no fault, output simply vanishes.  Driver threads run unprivileged
 * in this port, so allow secure unprivileged access to all APB/AHB peripheral
 * ports by setting every *SPPPC* control bit.
 */
#define SPCTRL_BASE		0x50080000u
#define SPCTRL_AHBSPPPC0	(SPCTRL_BASE + 0x90u)
#define SPCTRL_AHBSPPPCEXP0	(SPCTRL_BASE + 0xA0u)
#define SPCTRL_APBSPPPC0	(SPCTRL_BASE + 0xB0u)
#define SPCTRL_APBSPPPC1	(SPCTRL_BASE + 0xB4u)
#define SPCTRL_APBSPPPCEXP0	(SPCTRL_BASE + 0xC0u)

void ulmk_board_init(void)
{
	volatile uint32_t *sp;
	unsigned int       i;

	*(volatile uint32_t *)(uintptr_t)SPCTRL_AHBSPPPC0    = 0xFFFFFFFFu;
	*(volatile uint32_t *)(uintptr_t)SPCTRL_APBSPPPC0    = 0xFFFFFFFFu;
	*(volatile uint32_t *)(uintptr_t)SPCTRL_APBSPPPC1    = 0xFFFFFFFFu;

	sp = (volatile uint32_t *)(uintptr_t)SPCTRL_AHBSPPPCEXP0;
	for (i = 0u; i < 4u; i++)
		sp[i] = 0xFFFFFFFFu;

	sp = (volatile uint32_t *)(uintptr_t)SPCTRL_APBSPPPCEXP0;
	for (i = 0u; i < 4u; i++)
		sp[i] = 0xFFFFFFFFu;
}
