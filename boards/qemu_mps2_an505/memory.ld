/* boards/qemu_mps2_an505/memory.ld — Layer 3 chip input for QEMU mps2-an505.
 *
 * The AN505 (SSE-200 IoT-Kit) boots in Secure state and reads its reset vector
 * from the Secure code alias at 0x10000000, not 0x00000000.  This TrustZone-
 * unaware port runs entirely Secure, so every region uses the Secure alias
 * (IDAU bit 28 set).  Board RAM banks (QEMU an505_raminfo):
 *   ssram-0  0x00000000  4 MB  -> Secure 0x10000000  (code)
 *   ssram-1  0x28000000  2 MB  -> Secure 0x38000000  (kernel + user data)
 *   ssram-2  0x28200000  2 MB  -> Secure 0x38200000  (unused)
 * There is no usable RAM at 0x20000000, so data lives in ssram-1.
 */
MEMORY {
	KERNEL_FLASH  (rx)  : ORIGIN = 0x10000000, LENGTH = 512K
	KERNEL_RAM    (rwx) : ORIGIN = 0x38000000, LENGTH = 1024K
	SHARED_RAM    (rwx) : ORIGIN = 0x38100000, LENGTH = 256K
	PERIPH        (rw)  : ORIGIN = 0x50000000, LENGTH = 256M
}

ULMK_KERNEL_STACK_SIZE = 0x1000;
ULMK_ISR_STACK_SIZE    = 0x0800;
ULMK_MPU_ALIGN         = 32;

HAVE_CSA        = 0;
HAVE_SMALL_DATA = 0;
HAVE_BMHD       = 0;
