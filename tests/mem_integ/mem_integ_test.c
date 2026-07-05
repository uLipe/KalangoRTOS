/* SPDX-License-Identifier: MIT */
/*
 * Thread heap integration test — tests/mem_integ/mem_integ_test.c
 *
 * Exercises the slabAO per-thread heap model:
 *   ulmk_get_thread_heap() — query heap base and size
 *   ulmk_heap_extend()     — grow heap via kernel allocation
 *   Direct heap access     — verify DPR covers the heap area
 *
 * Root thread (KERNEL) spawns a child with heap_size > 0.  All memory
 * checks run inside the child.  Root waits for a notification then
 * prints the final verdict.
 *
 * Expected output contains "mem_integ: PASS".
 */

#include <stdint.h>
#include <stddef.h>
#include <ulmk/microkernel.h>
#include "../test_support.h"

#define HEAP_SIZE	4096u

static volatile int      g_pass;
static volatile int      g_fail;
static ulmk_notif_t      g_done_notif;

#define CHECK(cond) \
	do { \
		if (cond) { \
			g_pass++; \
		} else { \
			g_fail++; \
		} \
	} while (0)

static void heap_test_entry(void *arg)
{
	ulmk_heap_info_t info;
	volatile uint8_t *heap;
	size_t i;
	int rc;
	int ok;

	(void)arg;

	/* query heap info */
	rc = ulmk_get_thread_heap(&info);
	CHECK(rc == ULMK_OK);
	CHECK(info.base != 0u);
	CHECK(info.size == HEAP_SIZE);

	/* write-read pattern across the entire heap */
	heap = (volatile uint8_t *)(uintptr_t)info.base;
	for (i = 0; i < info.size; i++) {
		heap[i] = (uint8_t)(i & 0xFFu);
	}

	ok = 1;
	for (i = 0; i < info.size; i++) {
		if (heap[i] != (uint8_t)(i & 0xFFu)) {
			ok = 0;
			break;
		}
	}
	CHECK(ok);

	/* heap_extend adds a new accessible region */
	rc = ulmk_heap_extend(512u);
	CHECK(rc == ULMK_OK);

	ulmk_notif_signal(g_done_notif, 1u);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	uint32_t bits = 0u;

	(void)info;

	g_pass = 0;
	g_fail = 0;

	ulmk_printk("mem_integ: start\n");

	g_done_notif = ulmk_notif_create();

	attr.name       = "heap_test";
	attr.entry      = heap_test_entry;
	attr.arg        = NULL;
	attr.priority   = 1u;
	attr.stack_size = 1024u;
	attr.privilege  = ULMK_PRIV_DRIVER;
	attr.heap_size  = HEAP_SIZE;

	ulmk_thread_create(&attr);

	ulmk_notif_wait(g_done_notif, 1u, &bits);

	if (g_fail == 0) {
		ulmk_printk("mem_integ: PASS\n");
	} else {
		ulmk_printk("mem_integ: FAIL (pass=%d fail=%d)\n",
			    g_pass, g_fail);
	}

	ulmk_thread_exit();
}
