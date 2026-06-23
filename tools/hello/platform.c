/*
 * TriCore TC27x / QEMU platform layer — standalone hello world.
 *
 * Output uses the linumiz QEMU VIRT debug device at 0xBF000000:
 *   +0x0020: write byte  → putchar on QEMU stdout; write 0 → fflush
 *   +0x0028: write code  → exit(code) inside QEMU
 *
 * Source: hw/tricore/tricore_virt.c in qemu-aurix fork.
 */

#include <stdint.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * CSA pool — placed in .csa_pool (NOLOAD) section so BSS zeroing (below)
 * does not overwrite the chain built by startup.S.
 * -------------------------------------------------------------------------- */
#define CSA_COUNT 64

__attribute__((section(".csa_pool"), aligned(64)))
uint32_t tricore_csa_pool[CSA_COUNT * 16];

/* --------------------------------------------------------------------------
 * VIRT debug device
 * -------------------------------------------------------------------------- */
#define VIRT_BASE    0xBF000000UL
#define VIRT_PUTCHAR (*(volatile uint32_t *)(VIRT_BASE + 0x20U))
#define VIRT_EXIT    (*(volatile uint32_t *)(VIRT_BASE + 0x28U))

static void platform_putchar(char c)
{
    VIRT_PUTCHAR = (uint32_t)(uint8_t)c;
}

/* --------------------------------------------------------------------------
 * BSS zeroing — called from startup.S before main.
 * -------------------------------------------------------------------------- */
extern uint32_t __bss_start;
extern uint32_t __bss_end;

void platform_init(void)
{
    uint32_t *p;
    for (p = &__bss_start; p < &__bss_end; p++)
        *p = 0U;
    /* Make stdout unbuffered so printf calls _write() immediately. */
    setvbuf(stdout, NULL, _IONBF, 0);
}

/* --------------------------------------------------------------------------
 * Newlib syscall stubs
 * -------------------------------------------------------------------------- */
/*
 * Override POSIX write() from newlib/libgloss.
 * In this toolchain, _write_r() calls write() → ___virtio (debug trap).
 * Providing write() in an object file takes precedence over the archive
 * version, redirecting printf output to the VIRT device.
 */
int write(int fd, const char *buf, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++)
        platform_putchar(buf[i]);
    return len;
}

void _exit(int code)
{
    fflush(NULL);        /* drain all newlib stdio buffers via _write() */
    VIRT_PUTCHAR = 0U;   /* fflush QEMU's host stdout */
    VIRT_EXIT    = (uint32_t)code;
    for (;;)
        __asm volatile("nop");
}
