#if defined(CONFIG_ARCH_ARM_V6M)

        .thumb
        .syntax unified

        .extern current
        .extern isr_stack
        .extern CoreTaskSwitch
        .extern CoreSetRunning
        .global SVC_Handler
        .global PendSV_Handler
        .section .text

/* void DoStartKernel (void) */
        .thumb_func
SVC_Handler:
        ldr r0, =isr_stack
        movs r1, #CONFIG_ISR_STACK_SIZE 
        adds r0, r0, r1, lsl #2        
        msr msp, r0 // isr stack pointer is ready to use

        push {lr}
        bl CoreTaskSwitch
        pop {lr}
        ldr r1, [r0]

        /* ...after the callee regular registers */
        adds r1, r1, #16
        ldmia r1!, {r4 - r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7

        adds r1, r1, #-16
        ldmia r1!, {r4 - r7}

        adds r1, r1, #32

        /* horray the stack pointer is now handled to the CPU */
        msr psp, r1

        push {r2,lr}
        bl CoreSetRunning
        pop {r2,lr}

        /* re-enable interrupts and ensure return in thumb mode */
        mov r2, #0x04
        mov r1, lr
        orrs r1, r1, r2
        mov lr, r1
        bx lr

/* void DoContextSwitch (void) */
        .thumb_func
PendSV_Handler:

        /* disable interrupts during context switch */
        mrs r2, PRIMASK
        cpsid I

        /* Check if this is the first switch, skip register saving if does */
        ldr r0, =current 
        ldr r0, [r0] 
        mrs r1, psp

        /* push all registers to stack, incluing fp ones if needed */
        adds r1, r1, #-32
        stmia r1!, {r4 - r7}
        adds  r1, r1, #16

        mov r4, r8
        mov r5, r9
        mov r6, r10
        mov r7, r11
        stmia r1!, {r4 - r7}
        adds  r1, r1, #-16

        /* send stackpointer back to the tcb */
        str r1, [r0]

        push {lr}
        bl CoreTaskSwitch
        pop {lr}  
        ldr r1, [r0]

        /* restore next task callee registers */
        adds r1, r1, #16
        ldmia r1!, {r4 - r7}
        mov r8, r4
        mov r9, r5
        mov r10, r6
        mov r11, r7

        adds r1, r1, #-16
        ldmia r1!, {r4 - r7}
        adds r1, r1, #32

        /* horray the stack pointer is now handled to the CPU */
        msr psp, r1

        /* if the previous context saving was FP we need to tell the CPU to resume it*/        
        /* re-enable interrupts and ensure return in thumb mode */
        mov r2, #0x04
        mov r1, lr
        orrs r1, r1, r2
        mov lr, r1

        msr PRIMASK,r2
        bx lr
#endif

.end
