    .syntax unified
    .thumb

    .section .text, "ax"
    .extern arch_isr_entry
    .global ul_arch_isr_wrapper

ul_arch_isr_wrapper:
    ldr r0. =0xE000ED04
    ldr r0, [r0]
    and r0, r0, #0x1FF
    mrs r1, psp
    stmfd r1!, {r4 - r11}
    ldr r4, =arch_isr_entry
    blx r4

    bx lr
