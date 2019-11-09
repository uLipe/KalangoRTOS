#include <platform_qemu.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

#include "semihosting.h"

extern uint32_t _sbss;
extern uint32_t _ebss;

extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sidata;

void platform_init(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }

    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }
}

void platform_putchar(char c)
{
    semihost_call(SEMIHOST_SYS_WRITEC, &c);
}

void platform_exit(int code)
{
    //SYS_EXIT_EXTENDED takes a pointer to {reason, status}; the plain SYS_EXIT
    //reads R1 directly as the reason, so it cannot carry a process exit code.
    //field[0] is ADP_Stopped_ApplicationExit, field[1] the process exit status.
    uint32_t args[2] = { 0x20026U, (uint32_t)code };

    semihost_call(SEMIHOST_SYS_EXIT_EXTENDED, args);
    for (;;) {
        __asm volatile ("wfi");
    }
}

void _putchar(char c)
{
    platform_putchar(c);
}

int _write(int fd, const char *ptr, int len)
{
    (void)fd;

    if (!ptr || len <= 0) {
        return 0;
    }

    for (int i = 0; i < len; i++) {
        platform_putchar(ptr[i]);
    }

    return len;
}

void _exit(int code)
{
    platform_exit(code);
}

int _close(int fd)
{
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    if (st) {
        st->st_mode = S_IFCHR;
    }
    return 0;
}

int _isatty(int fd)
{
    (void)fd;
    return 1;
}

int _lseek(int fd, int ptr, int dir)
{
    (void)fd;
    (void)ptr;
    (void)dir;
    return 0;
}

int _read(int fd, char *ptr, int len)
{
    (void)fd;
    (void)ptr;
    (void)len;
    return 0;
}

void *_sbrk(int incr)
{
    extern char end;
    static char *heap_end = &end;
    char *prev = heap_end;

    heap_end += incr;
    return (void *)prev;
}
