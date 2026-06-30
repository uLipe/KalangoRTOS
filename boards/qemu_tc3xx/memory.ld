/* boards/qemu_tc3xx/memory.ld
 * Layer 3 — chip-specific input for QEMU KIT_AURIX_TC397B_TRB.
 * See docs/linker_spec.md §9 for the full contract.
 */
MEMORY {
	KERNEL_FLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 2M
	KERNEL_RAM    (rwx) : ORIGIN = 0x70000000, LENGTH = 240K
	SHARED_RAM    (rwx) : ORIGIN = 0x90040000, LENGTH = 256K
	PERIPH        (rw)  : ORIGIN = 0xF0000000, LENGTH = 256M
}

UL_CSA_POOL_SIZE     = 0x4000;
UL_KERNEL_STACK_SIZE = 0x1000;
UL_ISR_STACK_SIZE    = 0x0800;
UL_MPU_ALIGN         = 64;

HAVE_CSA        = 1;
HAVE_SMALL_DATA = 1;
HAVE_BMHD       = 0;
