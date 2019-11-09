.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector,"a",%progbits
.align 2

.global Reset_Handler
.extern Arch_CommonIsrEntry

.word _estack
.word Reset_Handler+1
.rept 30
.word Arch_CommonIsrEntry+1
.endr

.thumb_func
Reset_Handler:
    ldr sp, .L_stack_top
    bl platform_init
    bl main
1:  b 1b

.align 2
.L_stack_top:
.word _estack
