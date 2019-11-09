#include <stdint.h>
#include <string.h>

/*
 * TriCore QEMU platform layer.
 *
 * Semihosting output uses the TriCore debug trap via the QEMU monitor:
 * writing to the I/O character register at 0xF0000000+0x0F00 (ASC debug
 * register in the linumiz TC27x QEMU machine).  If the register is not
 * present, the write is silently dropped.
 *
 * BSS zeroing and .data initialisation are performed here so the startup
 * assembly can stay minimal.
 */

/* ---------------------------------------------------------------------------
 * Linker-provided symbols
 * --------------------------------------------------------------------------- */
extern uint32_t __bss_start;
extern uint32_t __bss_end;
extern uint32_t __data_start;
extern uint32_t __data_end;
extern uint32_t __data_load;   /* LMA of .data (in flash) */

/* ---------------------------------------------------------------------------
 * linumiz QEMU TriCore "VIRT" debug device — base 0xBF000000.
 *
 *   +0x0020: write byte → putchar to QEMU stdout; write 0 → fflush
 *   +0x0024: write N → usleep(1000*N) inside QEMU
 *   +0x0028: write code → exit(code) inside QEMU
 *
 * Source: hw/tricore/tricore_virt.c in the linumiz qemu-aurix fork.
 * --------------------------------------------------------------------------- */
#define TRICORE_VIRT_BASE       0xBF000000UL
#define TRICORE_VIRT_PUTCHAR    (*(volatile uint32_t *)(TRICORE_VIRT_BASE + 0x20U))
#define TRICORE_VIRT_EXIT       (*(volatile uint32_t *)(TRICORE_VIRT_BASE + 0x28U))

void platform_putchar(char c)
{
    TRICORE_VIRT_PUTCHAR = (uint32_t)(uint8_t)c;
}

void platform_exit(int code)
{
    TRICORE_VIRT_PUTCHAR = 0U;   /* flush stdout */
    TRICORE_VIRT_EXIT    = (uint32_t)code;
    for (;;) {
        __asm volatile("nop");
    }
}

/* ---------------------------------------------------------------------------
 * platform_trigger_test_irq — trigger a software interrupt at priority 16
 *
 * Uses IR SRC index 16 (address 0xF0038040) on the linumiz QEMU TC277 machine.
 * SRN_EN = bit 10 (SRE in TC3x mode); SETR = bit 26.
 *
 * The SRC must be pre-initialised with SRPN=16, SRE=1 before the first
 * trigger so the linumiz QEMU IR caches the correct priority.  Use a
 * read-modify-write (|=) to set SETR, matching the pattern used by
 * ArchSwIrqPend for the software-IRQ SRC.
 * --------------------------------------------------------------------------- */
#define TRICORE_SRC_BASE        0xF0038000UL
#define TRICORE_SRC_TEST_IRQ    (*(volatile uint32_t *)(TRICORE_SRC_BASE + 16U * 4U))
#define TRICORE_TEST_IRQ_PRIO   16U
#define TRICORE_SRN_EN          0x400U   /* bit 10 = SRE */
#define TRICORE_SRN_SETR        0x4000000U

static void platform_trigger_test_irq_init(void)
{
    TRICORE_SRC_TEST_IRQ = TRICORE_SRN_EN | TRICORE_TEST_IRQ_PRIO;
}

void platform_trigger_test_irq(void)
{
    TRICORE_SRC_TEST_IRQ = TRICORE_SRN_EN | TRICORE_TEST_IRQ_PRIO | TRICORE_SRN_SETR;
}

/* ---------------------------------------------------------------------------
 * platform_init — called from startup.S before main()
 *
 * startup.S copies .data from LMA (flash) to VMA (SRAM) before calling here.
 * This function only zeroes BSS; QEMU's zero-initialized RAM means BSS is
 * already zeroed, but the explicit loop is kept for correctness.
 * --------------------------------------------------------------------------- */
void platform_init(void)
{
    uint32_t *p;
    for (p = &__bss_start; p < &__bss_end; p++) {
        *p = 0U;
    }
    /* Pre-configure the test SRC with SRPN=16, SRE=1 so the linumiz QEMU IR
     * caches the correct priority before platform_trigger_test_irq uses it. */
    platform_trigger_test_irq_init();
}

/* ---------------------------------------------------------------------------
 * Minimal newlib syscall stubs
 * --------------------------------------------------------------------------- */
void _putchar(char c)
{
    platform_putchar(c);
}

int _write(int fd, const char *buf, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        platform_putchar(buf[i]);
    }
    return len;
}

void _exit(int code)
{
    platform_exit(code);
}
