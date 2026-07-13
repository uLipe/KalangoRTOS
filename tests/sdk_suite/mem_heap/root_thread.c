/* SPDX-License-Identifier: MIT */
#include "sdk_test_util.h"

#define HEAP_SIZE	4096u

static volatile int g_pass;
static volatile int g_fail;
static ulmk_notif_t g_done;

#define CHECK(cond) do { if (cond) g_pass++; else g_fail++; } while (0)

static void heap_test(void *arg)
{
	ulmk_heap_info_t info;
	volatile uint8_t *heap;
	size_t           i;
	int              ok;

	(void)arg;
	CHECK(ulmk_get_thread_heap(&info) == ULMK_OK);
	CHECK(info.base != 0u);
	CHECK(info.size == HEAP_SIZE);

	heap = (volatile uint8_t *)(uintptr_t)info.base;
	for (i = 0; i < info.size; i++)
		heap[i] = (uint8_t)(i & 0xFFu);
	ok = 1;
	for (i = 0; i < info.size; i++) {
		if (heap[i] != (uint8_t)(i & 0xFFu)) {
			ok = 0;
			break;
		}
	}
	CHECK(ok);
	CHECK(ulmk_heap_extend(512u) == ULMK_OK);

	ulmk_notif_signal(g_done, 1u);
	ulmk_thread_exit();
}

void ulmk_root_thread(const ulmk_boot_info_t *info)
{
	uint32_t bits = 0u;

	board_services_init(info);
	sdk_puts("mem_heap: start\n");
	g_done = ulmk_notif_create();
	sdk_spawn("heap", heap_test, NULL, 1u, 1024u, HEAP_SIZE);
	ulmk_notif_wait(g_done, 1u, &bits);

	if (g_fail == 0)
		sdk_puts("mem_heap: PASS\n");
	else
		sdk_puts("mem_heap: FAIL\n");
	ulmk_thread_exit();
}
