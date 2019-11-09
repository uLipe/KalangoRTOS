/*
 * TriCore TC27x / QEMU startup
 *
 * Entered directly after reset at the address stored in the boot flash
 * header (BMHD) or by QEMU at load time.  Initialises the stack pointer,
 * the CSA free list, and jumps to the C platform_init / kernel entry.
 *
 * No ENDINIT watchdog handling is needed for QEMU (the PW mechanism is not
 * emulated).  On real hardware, insert an ENDINIT unlock sequence before
 * writing protected registers.
 */

#include <KalangoRTOS/kalango_config_internal.h>

    .extern __stack_top
    .extern __data_load
    .extern __data_start
    .extern __data_end
    .extern platform_init
    .extern tricore_csa_pool

    .section .startup.text, "ax"
    .global _start
    .type   _start, @function
_start:
    /* Set the initial stack pointer (A10) to the top of the C stack. */
    movh.a  %a10, hi:__stack_top
    lea     %a10, [%a10] lo:__stack_top

    /*
     * Build the CSA free list from tricore_csa_pool before any CALL.
     * QEMU resets FCX to 0; a CALL with FCX==0 raises an FCU trap (class 3,
     * TIN3) which, with BTV still at 0, crashes into unmapped memory silently.
     *
     * tricore_csa_pool is in .csa_pool (NOLOAD), not .bss, so the BSS
     * zeroing inside platform_init does not destroy the chain.
     *
     * ADDR_TO_CSA for SRAM (0x7xxxxxxx):
     *   link = 0x00070000 | extr.u(addr, 6, 16)
     *   where extr.u(addr, 6, 16) extracts addr[21:6] zero-extended.
     */
    movh.a  %a2, hi:tricore_csa_pool
    lea     %a2, [%a2] lo:tricore_csa_pool
    lea     %a3, [%a2] 0                    /* a3 = &pool[current] */
    mov     %d4, (CONFIG_TRICORE_CSA_COUNT - 1)

.L_csa_link:
    lea     %a4, [%a3] 64                   /* a4 = &pool[next] (each CSA = 64 B) */
    mov.d   %d0, %a4
    extr.u  %d0, %d0, 6, 16                /* bits[21:6] = offset in CSA word */
    movh    %d1, 7                          /* 0x00070000 = SRAM segment mask */
    or      %d0, %d1
    st.w    [%a3], %d0                      /* pool[current][0] = link to next */
    lea     %a3, [%a4] 0                    /* advance current to next */
    add     %d4, -1
    jnz     %d4, .L_csa_link

    mov     %d0, 0
    st.w    [%a3], %d0                      /* pool[last][0] = 0 (end of chain) */

    /* FCX = ADDR_TO_CSA(&pool[0]) */
    mov.d   %d0, %a2
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    dsync
    mtcr    0xFE38, %d0                     /* FCX */
    isync

    /* LCX = ADDR_TO_CSA(&pool[count-2]) — triggers FCD before full exhaustion */
    lea     %a5, [%a3] -64
    mov.d   %d0, %a5
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    dsync
    mtcr    0xFE3C, %d0                     /* LCX */
    isync

    /*
     * Copy .data from flash LMA to SRAM VMA.
     * QEMU loads ELF PT_LOAD segments to p_paddr (LMA), not VMA.
     * Initialised data (e.g. linked-list heads) must be copied before use.
     */
    movh.a  %a5, hi:__data_load
    lea     %a5, [%a5] lo:__data_load
    movh.a  %a6, hi:__data_start
    lea     %a6, [%a6] lo:__data_start
    movh.a  %a7, hi:__data_end
    lea     %a7, [%a7] lo:__data_end
.L_data_copy:
    mov.d   %d0, %a6
    mov.d   %d1, %a7
    jge     %d0, %d1, .L_data_done
    ld.w    %d0, [%a5]
    st.w    [%a6], %d0
    lea     %a5, [%a5] 4
    lea     %a6, [%a6] 4
    j       .L_data_copy
.L_data_done:

    /* Zero BSS. */
    call    platform_init

    /* Enter the application entry point. */
    call    main

.L_hang:
    j       .L_hang
