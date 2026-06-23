    .syntax unified
    .thumb
    .section .text, "ax"
    .global ul_arch_kmicrokernel_start
    .extern ul_arch_vt

ul_arch_kmicrokernel_start:
    ldr r0, =ul_arch_vt
    ldr r1, =0xE000ED08 /* Vector table offset register */
    str r0, [r1]

    ldr r0, =0xE000ED14 /* CCR stack align bits  to double words*/
    ldr r1, [r0]
    orr r1, r1, #(1 << 9)
    str r1, [r0]

    movs r1, #0xFF  /* Set priorities for pendSV, SVC and Systick */
    ldr r0, =0xE000ED22
    strb r1, [r0]
    ldr r0, =0xE000ED23
    strb r1, [r0]
    ldr r0, =0xE000ED1B
    strb r1, [r0]

    cpsie i
    svc #0  /* all good, start kernel using syscall #0 */

