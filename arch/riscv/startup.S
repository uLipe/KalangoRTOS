/* SPDX-License-Identifier: MIT */
/*
 * RISC-V RV32 kernel startup — arch/riscv/startup.S
 */

	.extern _ulmk_kernel_stack_top
	.extern __ulmk_kernel_data_load
	.extern _ulmk_kernel_data_start
	.extern _ulmk_kernel_data_end
	.extern _ulmk_kernel_bss_start
	.extern _ulmk_kernel_bss_end
	.extern __ulmk_user_data_load
	.extern _ulmk_user_data_start
	.extern _ulmk_user_data_end
	.extern _ulmk_user_pool_start
	.extern ulmk_board_init
	.extern ulmk_arch_init
	.extern ulmk_kern_main

	.section .bss
	.align  4
boot_info:
	.space  64

	.section .text._start, "ax", @progbits
	.global _start
	.type _start, @function
_start:
	la      sp, _ulmk_kernel_stack_top

	call    ulmk_board_init

	la      t0, __ulmk_kernel_data_load
	la      t1, _ulmk_kernel_data_start
	la      t2, _ulmk_kernel_data_end
1:
	bge     t1, t2, 2f
	lw      t3, 0(t0)
	sw      t3, 0(t1)
	addi    t0, t0, 4
	addi    t1, t1, 4
	j       1b
2:
	la      t1, _ulmk_kernel_bss_start
	la      t2, _ulmk_kernel_bss_end
3:
	bge     t1, t2, 4f
	sw      zero, 0(t1)
	addi    t1, t1, 4
	j       3b
4:
	la      t0, __ulmk_user_data_load
	la      t1, _ulmk_user_data_start
	la      t2, _ulmk_user_data_end
5:
	bge     t1, t2, 6f
	lw      t3, 0(t0)
	sw      t3, 0(t1)
	addi    t0, t0, 4
	addi    t1, t1, 4
	j       5b
6:
	la      t1, _ulmk_user_data_end
	la      t2, _ulmk_user_pool_start
7:
	bge     t1, t2, 8f
	sw      zero, 0(t1)
	addi    t1, t1, 4
	j       7b
8:
	la      sp, _ulmk_kernel_stack_top
	addi    sp, sp, -16
	la      a0, boot_info
	call    ulmk_arch_init
	la      a0, boot_info
	call    ulmk_kern_main
9:
	j       9b

	.size _start, . - _start
