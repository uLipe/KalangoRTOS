/* SPDX-License-Identifier: MIT */
/*
 * Host unit test for ulmk_arch_atomic_* semantics (mocked).
 * Replaces atomic_integ phase 1 (pre_root_hook).
 */
#include <stdint.h>
#include <stdio.h>

static int g_pass;
static int g_fail;

#define CHECK(cond, msg) \
	do { \
		if (cond) { \
			printf("  [PASS] %s\n", msg); \
			g_pass++; \
		} else { \
			printf("  [FAIL] %s\n", msg); \
			g_fail++; \
		} \
	} while (0)

static uint32_t ulmk_arch_atomic_cas(volatile uint32_t *ptr,
				     uint32_t expected, uint32_t desired)
{
	uint32_t old = *ptr;

	if (old == expected)
		*ptr = desired;
	return old;
}

static uint32_t ulmk_arch_atomic_add(volatile uint32_t *ptr, uint32_t val)
{
	uint32_t old = *ptr;

	*ptr = old + val;
	return old;
}

#define PHASE1_ITERS 10000u

int main(void)
{
	volatile uint32_t counter;
	uint32_t          old;
	uint32_t          i;

	counter = 0u;
	for (i = 0u; i < PHASE1_ITERS; i++)
		ulmk_arch_atomic_add(&counter, 1u);
	CHECK(counter == PHASE1_ITERS, "atomic_add loop");

	counter = 42u;
	old = ulmk_arch_atomic_cas(&counter, 42u, 99u);
	CHECK(old == 42u && counter == 99u, "atomic_cas match");

	old = ulmk_arch_atomic_cas(&counter, 0u, 1u);
	CHECK(old == 99u && counter == 99u, "atomic_cas mismatch");

	printf("ATOMIC UNIT: %s (%d/%d)\n",
	       g_fail ? "FAIL" : "PASS", g_pass, g_pass + g_fail);
	return g_fail ? 1 : 0;
}
