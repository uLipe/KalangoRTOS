#if defined(CONFIG_ARCH_ARM_V7M)

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

#if defined(CONFIG_HAS_FLOAT)
        ldmfd r1!, {r3}
#endif       
        /* ...after the callee regular registers */
        ldmfd r1!, {r4 - r11}

        /* horray the stack pointer is now handled to the CPU */
        msr psp, r1

        push {r2,lr}
        bl CoreSetRunning
        pop {r2,lr}

        /* re-enable interrupts and ensure return in thumb mode */
        orr lr, lr, #0x04
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
#if defined(CONFIG_HAS_FLOAT)
        tst lr, #0x10
        it  eq
        vstmdbeq r1!, {d8 - d15}
#endif       

        stmfd r1!, {r4 - r11}

        /* for fp context we need to store that there are a fp active context */
#if defined(CONFIG_HAS_FLOAT)
        mov r4, #0x00
        tst lr, #0x10
        it  eq
        moveq r4, #0x01
        stmfd r1!, {r4} 
#endif       
        /* send stackpointer back to the tcb */
        str r1, [r0]

        push {lr}
        bl CoreTaskSwitch
        pop {lr}  
        ldr r1, [r0]

        /* same here, if a fp context was active, restore the fp registers */
#if defined(CONFIG_HAS_FLOAT)
        ldmfd r1!, {r3}
#endif       
        /* ...after the callee regular registers */
        ldmfd r1!, {r4 - r11}

#if defined(CONFIG_HAS_FLOAT)
        cmp r3, #0x00
        it ne
        vldmiane r1!, {d8 - d15}
#endif       
        /* horray the stack pointer is now handled to the CPU */
        msr psp, r1

        /* if the previous context saving was FP we need to tell the CPU to resume it*/
#if defined(CONFIG_HAS_FLOAT)
        orr lr, lr, #0x10
        cmp r3, #0x00
        it ne
        bicne lr, lr, #0x10
#endif       
        
        /* re-enable interrupts and ensure return in thumb mode */
        orr lr, lr, #0x04
        msr PRIMASK,r2
        bx lr
#endif

.end
