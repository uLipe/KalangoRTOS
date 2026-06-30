/*
 * TriCore TC27x / QEMU startup — standalone (no RTOS dependency).
 *
 * Sequence:
 *   1. Set stack pointer (A10) to top of SRAM stack region.
 *   2. Build CSA free list from tricore_csa_pool before the first CALL.
 *      QEMU resets FCX to 0; a CALL with FCX==0 raises an FCU trap (class 3)
 *      which silently crashes if BTV is still 0.
 *   3. Copy .data from flash LMA to SRAM VMA.
 *   4. Call platform_init (zeroes BSS).
 *   5. Call main; pass its return value to _exit.
 */

/* Number of 64-byte CSA blocks in tricore_csa_pool (defined in platform.c). */
.set CSA_COUNT, 64

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
    /* 1. Set stack pointer. */
    movh.a  %a10, hi:__stack_top
    lea     %a10, [%a10] lo:__stack_top

    /* 2. Build CSA free list.
     *
     * ADDR_TO_CSA for SRAM (0x7xxxxxxx):
     *   link = 0x00070000 | extr.u(addr, 6, 16)
     *   where extr.u(addr, 6, 16) extracts addr[21:6] zero-extended.
     */
    movh.a  %a2, hi:tricore_csa_pool
    lea     %a2, [%a2] lo:tricore_csa_pool
    lea     %a3, [%a2] 0                    /* a3 = &pool[current] */
    mov     %d4, (CSA_COUNT - 1)

.L_csa_link:
    lea     %a4, [%a3] 64                   /* a4 = &pool[next] */
    mov.d   %d0, %a4
    extr.u  %d0, %d0, 6, 16                /* bits[21:6] */
    movh    %d1, 7                          /* 0x00070000 = SRAM segment */
    or      %d0, %d1
    st.w    [%a3], %d0                      /* pool[current][0] = link to next */
    lea     %a3, [%a4] 0
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

    /* LCX = ADDR_TO_CSA(&pool[count-2]) — triggers FCD before full exhaustion. */
    lea     %a5, [%a3] -64
    mov.d   %d0, %a5
    extr.u  %d0, %d0, 6, 16
    movh    %d1, 7
    or      %d0, %d1
    dsync
    mtcr    0xFE3C, %d0                     /* LCX */
    isync

    /* 3. Copy .data from flash (LMA) to SRAM (VMA). */
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

    /* 4. BSS zeroing. */
    call    platform_init

    /* 5. Run application; propagate exit code. */
    call    main
    mov     %d4, %d2                        /* D2 = main() return value; D4 = _exit() arg */
    /*
     * Pre-load D15 with the TSIM "test passed" sentinel (0x900d).
     * CALL preserves live D-registers across the call boundary; the first
     * instruction of _exit reads D15 to set A14 before TSIM checks it.
     * For QEMU builds this is a harmless no-op (VIRT device handles exit).
     */
    mov.u   %d15, 0x900d
    call    _exit

.L_hang:
    j       .L_hang
