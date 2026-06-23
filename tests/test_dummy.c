/*
 * Dummy test suite — validates that the build, QEMU boot, and Unity I/O
 * pipeline are all working before any kernel code is written.
 *
 * These tests must always pass.  If they fail, something is wrong with the
 * toolchain, linker script, platform layer, or QEMU configuration.
 */

#include "unity.h"

void test_dummy_one_plus_one(void)
{
	TEST_ASSERT_EQUAL_INT(2, 1 + 1);
}

void test_dummy_qemu_output(void)
{
	/*
	 * If this test is reported by Unity, the write() → VIRT putchar
	 * pipeline is working: printf output reaches the QEMU console.
	 */
	TEST_ASSERT_TRUE(1);
}
