/* boards/qemu_riscv_virt_smp4/memory.ld — Layer 3 for QEMU rv32 virt (4-hart). */
MEMORY {
	KERNEL_FLASH  (rx)  : ORIGIN = 0x80000000, LENGTH = 256K
	KERNEL_RAM    (rwx) : ORIGIN = 0x80040000, LENGTH = 768K
	SHARED_RAM    (rwx) : ORIGIN = 0x90000000, LENGTH = 64K
	PERIPH        (rw)  : ORIGIN = 0x00000000, LENGTH = 512M
}

ULMK_KERNEL_STACK_SIZE = 0x1000;
ULMK_ISR_STACK_SIZE    = 0x0800;
ULMK_MPU_ALIGN         = 64;

HAVE_CSA        = 0;
HAVE_SMALL_DATA = 0;
HAVE_BMHD       = 0;
