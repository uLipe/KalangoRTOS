/* SPDX-License-Identifier: MIT */
/*
 * Weak no-op — boards override with board_wdt_early.S (ji %a11 return).
 */

	.section .text.board_wdt_early, "ax"
	.weak   ulmk_board_wdt_disable_early
	.type   ulmk_board_wdt_disable_early, @function
ulmk_board_wdt_disable_early:
	ji      %a11
	.size   ulmk_board_wdt_disable_early, . - ulmk_board_wdt_disable_early
