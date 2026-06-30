/* boards/tsim_tc27x/memory.ld
 * Layer 3 — chip-specific input for the TSIM TC27x simulation target.
 * Consumed by generate_ld.py via the --chip-dir argument.
 *
 * Memory layout mirrors TC27x hardware addresses as defined in MConfig:
 *   PFLASH0 NC alias : 2 MiB at 0x80000000
 *   DSPR0 Core0      : 240 KiB at 0x70000000
 *   LMU shared RAM   : 32 KiB  at 0x90000000
 */

MEMORY {
    KERNEL_FLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 2M
    KERNEL_RAM    (rwx) : ORIGIN = 0x70000000, LENGTH = 240K
    SHARED_RAM    (rwx) : ORIGIN = 0x90000000, LENGTH = 32K
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
HAVE_BMHD       = 0;
