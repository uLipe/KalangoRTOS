/* SPDX-License-Identifier: MIT */
/*
 * Heap integration test — tests/mem_integ/mem_integ_test.c
 *
 * Exercises ulmk_malloc / ulmk_free / ulmk_aligned_alloc through the syscall layer.
 * Expected output contains "MEM INTEG TEST: PASS".
 */

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>
#include <kernel/include/ulmk_printk.h>

static int g_pass;
static int g_fail;

#define CHECK(cond, msg) \
	do { \
		if (cond) { \
			ulmk_printk("  [PASS] " msg "\n"); \
			g_pass++; \
		} else { \
			ulmk_printk("  [FAIL] " msg "\n"); \
			g_fail++; \
		} \
	} while (0)

static void test_basic_malloc(void)
{
	void *p;

	ulmk_printk("test_basic_malloc\n");

	p = ulmk_malloc(64);
	CHECK(p != NULL, "ulmk_malloc(64) returns non-NULL");
	CHECK(((uintptr_t)p % 64) == 0, "result is 64-byte aligned");
	ulmk_free(p);
}

static void test_multiple_allocs(void)
{
	void *ptrs[4];
	int   i;
	int   ok;

	ulmk_printk("test_multiple_allocs\n");

	for (i = 0; i < 4; i++)
		ptrs[i] = ulmk_malloc(128);

	ok = 1;
	for (i = 0; i < 4; i++) {
		if (!ptrs[i])
			ok = 0;
	}
	CHECK(ok, "four concurrent 128-byte allocs all non-NULL");

	for (i = 0; i < 4; i++)
		ulmk_free(ptrs[i]);
}

static void test_free_and_realloc(void)
{
	void *p1;
	void *p2;

	ulmk_printk("test_free_and_realloc\n");

	p1 = ulmk_malloc(256);
	CHECK(p1 != NULL, "initial alloc");
	ulmk_free(p1);

	p2 = ulmk_malloc(256);
	CHECK(p2 != NULL, "alloc after free succeeds");
	ulmk_free(p2);
}

static void test_aligned_alloc(void)
{
	void *p;

	ulmk_printk("test_aligned_alloc\n");

	p = ulmk_aligned_alloc(128, 64);
	CHECK(p != NULL, "ulmk_aligned_alloc(128, 64) non-NULL");
	CHECK(((uintptr_t)p % 128) == 0, "result is 128-byte aligned");
	ulmk_free(p);
}

static void run_tests(void)
{
	g_pass = 0;
	g_fail = 0;

	test_basic_malloc();
	test_multiple_allocs();
	test_free_and_realloc();
	test_aligned_alloc();

	if (g_fail == 0)
		ulmk_printk("MEM INTEG TEST: PASS\n");
	else
		ulmk_printk("MEM INTEG TEST: FAIL\n");
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	(void)info;
	run_tests();
	ulmk_thread_exit();
}
