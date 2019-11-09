#define VIRT_PUTCHAR (*(volatile unsigned int *)(0xBF000020UL))
#define VIRT_EXIT    (*(volatile unsigned int *)(0xBF000028UL))

void _start(void) __attribute__((noreturn));
void _start(void) {
    VIRT_PUTCHAR = 'H';
    VIRT_PUTCHAR = 'e';
    VIRT_PUTCHAR = 'l';
    VIRT_PUTCHAR = 'l';
    VIRT_PUTCHAR = 'o';
    VIRT_PUTCHAR = '\n';
    VIRT_PUTCHAR = 0;
    VIRT_EXIT = 0;
    while(1) {}
}
