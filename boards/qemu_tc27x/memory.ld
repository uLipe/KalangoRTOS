/* boards/qemu_tc27x/memory.ld
 * Layer 3 — chip-specific input (kernel-internal QEMU platform).
 * Consumed by generate_ld.py via the --chip-dir argument.
 *
 * TC27x as emulated by Linumiz QEMU-AURIX (qemu-system-tricore -machine tricore_testboard)
 * PFLASH0: 2 MiB at 0x80000000 (cached PFlash alias)
 * LMU RAM: 1 MiB at 0x90000000 (used as SRAM in QEMU)
 * VIRT debug device: 0xBF000000 (semihosting / console output)
 */

MEMORY {
    KERNEL_FLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 2M
    KERNEL_RAM    (rwx) : ORIGIN = 0x90000000, LENGTH = 1M
}

/* CSA pool: 256 frames × 64 bytes = 16 KiB */
UL_CSA_POOL_SIZE     = 0x4000;

/* Stack sizes */
UL_KERNEL_STACK_SIZE = 0x1000;   /* 4 KiB */
UL_ISR_STACK_SIZE    = 0x0800;   /* 2 KiB */

/* MPU region alignment (must match arch_config.h UL_ARCH_REGION_ALIGN) */
UL_MPU_ALIGN = 64;

/* Arch feature flags consumed by generate_ld.py */
HAVE_CSA        = 1;
HAVE_SMALL_DATA = 1;
HAVE_BMHD       = 0;   /* QEMU does not need a Boot Mode Header */
