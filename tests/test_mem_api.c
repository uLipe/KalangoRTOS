/*
 * Memory API test suite — covers ul_mem_map (anonymous and peripheral),
 * ul_mem_unmap, and ul_mem_grant.
 *
 * All tests are inside #if 0 until the kernel is implemented.
 */

#include "unity.h"
#include <stddef.h>
#include <stdint.h>

/* #include <sys/ulipe_microkernel.h> */

#if 0

/* =========================================================================
 * ul_mem_map — anonymous (UL_MMAP_ANON)
 * ========================================================================= */

/* HAPPY PATH */

void test_mem_map_anon_returns_non_null(void)
{
	void *p = ul_mem_map(NULL, 256, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
}

void test_mem_map_anon_result_is_8byte_aligned(void)
{
	void *p = ul_mem_map(NULL, 256, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_UINT32(0u, (uintptr_t)p & 0x7u);
}

void test_mem_map_anon_region_is_readable(void)
{
	volatile uint8_t *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
					 UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	(void)*p;  /* must not fault */
	TEST_ASSERT_TRUE(1);
}

void test_mem_map_anon_region_is_writable(void)
{
	volatile uint32_t *p = ul_mem_map(NULL, 64,
					  UL_PERM_READ | UL_PERM_WRITE,
					  UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	*p = 0xDEADBEEFu;
	TEST_ASSERT_EQUAL_HEX32(0xDEADBEEFu, *p);
}

void test_mem_map_anon_region_is_zeroed(void)
{
	/*
	 * The kernel must zero anonymous regions before handing them out
	 * to prevent information leakage between processes.
	 */
	volatile uint8_t *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
					 UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	for (int i = 0; i < 64; i++)
		TEST_ASSERT_EQUAL_UINT8(0u, p[i]);
}

void test_mem_map_anon_read_only_perms(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ, UL_MMAP_ANON);

	TEST_ASSERT_NOT_NULL(p);
}

void test_mem_map_anon_two_regions_do_not_overlap(void)
{
	uintptr_t a = (uintptr_t)ul_mem_map(NULL, 64,
					    UL_PERM_READ | UL_PERM_WRITE,
					    UL_MMAP_ANON);
	uintptr_t b = (uintptr_t)ul_mem_map(NULL, 64,
					    UL_PERM_READ | UL_PERM_WRITE,
					    UL_MMAP_ANON);

	TEST_ASSERT_NOT_EQUAL(0u, a);
	TEST_ASSERT_NOT_EQUAL(0u, b);
	/* Non-overlapping: one ends before the other starts. */
	TEST_ASSERT_TRUE((a + 64 <= b) || (b + 64 <= a));
}

/* EDGE CASES */

void test_mem_map_size_1_rounds_up_to_alignment(void)
{
	/*
	 * size=1 is below UL_ARCH_REGION_ALIGN (8).  The kernel rounds up;
	 * the call must succeed and return an 8-byte-aligned pointer.
	 */
	void *p = ul_mem_map(NULL, 1, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_UINT32(0u, (uintptr_t)p & 0x7u);
}

void test_mem_map_size_8_exact_alignment(void)
{
	void *p = ul_mem_map(NULL, 8, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_UINT32(0u, (uintptr_t)p & 0x7u);
}

void test_mem_map_wx_write_exec_strips_exec(void)
{
	/*
	 * W^X policy: a region with WRITE must not be executable.
	 * The kernel strips EXEC when WRITE is present.  The call itself
	 * must still succeed.
	 */
	void *p = ul_mem_map(NULL, 64,
			     UL_PERM_READ | UL_PERM_WRITE | UL_PERM_EXEC,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
}

void test_mem_map_exec_only_no_write_allowed(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_EXEC,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
}

/* CRASH PREVENTION */

void test_mem_map_size_zero_returns_null(void)
{
	void *p = ul_mem_map(NULL, 0, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NULL(p);
}

void test_mem_map_no_perms_returns_null(void)
{
	void *p = ul_mem_map(NULL, 64, 0u, UL_MMAP_ANON);

	TEST_ASSERT_NULL(p);
}

void test_mem_map_exhausts_pool_returns_null(void)
{
	/*
	 * Request very large allocations until the physical pool is depleted.
	 * Each failure must return NULL, never fault.
	 */
	int succeeded = 0;
	for (int i = 0; i < 64; i++) {
		void *p = ul_mem_map(NULL, 65536u,
				     UL_PERM_READ | UL_PERM_WRITE,
				     UL_MMAP_ANON);
		if (p)
			succeeded++;
		else
			break;
	}
	/* At least one allocation before exhaustion. */
	TEST_ASSERT_GREATER_THAN_INT(0, succeeded);
}

/* =========================================================================
 * ul_mem_map — peripheral (UL_MMAP_PERIPH)
 * ========================================================================= */

/* CRASH PREVENTION */

void test_mem_map_periph_without_cap_returns_eperm(void)
{
	/*
	 * A user thread (IO=0) has no peripheral capabilities.
	 * Any PERIPH map attempt must be rejected.
	 */
	void *p = ul_mem_map((void *)0xF0000600u, 256,
			     UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_PERIPH);
	TEST_ASSERT_NULL(p);
}

void test_mem_map_periph_unknown_base_returns_null(void)
{
	/*
	 * Address not in the kernel's peripheral table must be rejected
	 * even if the caller holds a capability.
	 */
	void *p = ul_mem_map((void *)0x12345678u, 256,
			     UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_PERIPH);
	TEST_ASSERT_NULL(p);
}

void test_mem_map_periph_null_base_returns_null(void)
{
	void *p = ul_mem_map(NULL, 256,
			     UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_PERIPH);
	TEST_ASSERT_NULL(p);
}

/* =========================================================================
 * ul_mem_unmap
 * ========================================================================= */

/* HAPPY PATH */

void test_mem_unmap_valid_ptr_returns_zero(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_INT(0, ul_mem_unmap(p, 64));
}

void test_mem_map_unmap_cycle_repeated(void)
{
	for (int i = 0; i < 4; i++) {
		void *p = ul_mem_map(NULL, 64,
				     UL_PERM_READ | UL_PERM_WRITE,
				     UL_MMAP_ANON);
		TEST_ASSERT_NOT_NULL(p);
		TEST_ASSERT_EQUAL_INT(0, ul_mem_unmap(p, 64));
	}
}

/* CRASH PREVENTION */

void test_mem_unmap_null_addr_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_unmap(NULL, 64));
}

void test_mem_unmap_unknown_addr_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_unmap((void *)0xDEADBEEFu, 64));
}

void test_mem_unmap_double_free_returns_error(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_INT(0, ul_mem_unmap(p, 64));
	/* Second unmap: must return an error, not corrupt the allocator. */
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_unmap(p, 64));
}

void test_mem_unmap_size_zero_returns_error(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_unmap(p, 0));
}

/* =========================================================================
 * ul_mem_grant
 * ========================================================================= */

/* HAPPY PATH */

void test_mem_grant_read_only_from_rw_source_returns_zero(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);

	/* Grant read-only to self (simplest valid target for unit test). */
	int ret = ul_mem_grant(p, 64, ul_thread_self(), UL_PERM_READ);

	TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_mem_grant_same_perms_as_source_returns_zero(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);

	int ret = ul_mem_grant(p, 64, ul_thread_self(),
			       UL_PERM_READ | UL_PERM_WRITE);
	TEST_ASSERT_EQUAL_INT(0, ret);
}

/* EDGE CASES */

void test_mem_grant_perms_clamped_to_source(void)
{
	/*
	 * Granting WRITE on a READ-ONLY region must not be permitted.
	 * The kernel should either clamp to source perms or return an error.
	 */
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ, UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);

	int ret = ul_mem_grant(p, 64, ul_thread_self(),
			       UL_PERM_READ | UL_PERM_WRITE);
	/*
	 * Two valid outcomes:
	 *   a) Returns 0 but quietly strips WRITE (W^X-style clamp).
	 *   b) Returns -UL_EPERM (grant would elevate permissions).
	 * Either is acceptable; crash / silent elevation is not.
	 */
	TEST_ASSERT_TRUE(ret == 0 || ret == -UL_EPERM);
}

/* CRASH PREVENTION */

void test_mem_grant_null_addr_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_grant(NULL, 64,
						  ul_thread_self(),
						  UL_PERM_READ));
}

void test_mem_grant_unknown_addr_returns_error(void)
{
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_grant((void *)0xDEADBEEFu, 64,
						  ul_thread_self(),
						  UL_PERM_READ));
}

void test_mem_grant_invalid_target_returns_error(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_grant(p, 64,
						  UL_TID_INVALID,
						  UL_PERM_READ));
}

void test_mem_grant_zero_size_returns_error(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_grant(p, 0,
						  ul_thread_self(),
						  UL_PERM_READ));
}

void test_mem_grant_unmapped_region_returns_error(void)
{
	void *p = ul_mem_map(NULL, 64, UL_PERM_READ | UL_PERM_WRITE,
			     UL_MMAP_ANON);
	TEST_ASSERT_NOT_NULL(p);
	TEST_ASSERT_EQUAL_INT(0, ul_mem_unmap(p, 64));

	/* Grant after unmap must fail gracefully. */
	TEST_ASSERT_LESS_THAN_INT(0, ul_mem_grant(p, 64,
						  ul_thread_self(),
						  UL_PERM_READ));
}

#endif /* 0 */
