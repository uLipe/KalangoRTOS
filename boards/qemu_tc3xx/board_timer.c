/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * Board timer service — boards/qemu_tc3xx/board_timer.c
 *
 * Userspace timer policy for the QEMU TC3xx target: maps STM0, binds the
 * STM0 SR0 service request to a notification, and serves sleep requests
 * over IPC.  The kernel delivers the compare-match IRQ via notif only.
 *
 * Hardware setup runs in board_timer_start() on the root thread so STM0 and
 * the IRQ binding are ready before the server thread's first syscall.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include "board_config.h"
#include "board_timer.h"

#ifndef BOARD_TIMER_STM0_BASE
#define BOARD_TIMER_STM0_BASE	ULMK_BOARD_STM0_BASE
#endif

#ifndef BOARD_TIMER_SRC_STM0_SR0
#define BOARD_TIMER_SRC_STM0_SR0	ULMK_BOARD_SRC_STM0_SR0
#endif

#ifndef BOARD_TIMER_HW_CLOCK_HZ
#define BOARD_TIMER_HW_CLOCK_HZ	50000000u
#endif

#define BOARD_TIMER_SRPN		1u
#define BOARD_TIMER_STM0_MAP_SIZE	0x80u
#define BOARD_TIMER_NOTIF_BIT		0u
#define BOARD_TIMER_MSG_SLEEP		1u

#define STM0_TIM0	(BOARD_TIMER_STM0_BASE + 0x010u)
#define STM0_CMP0	(BOARD_TIMER_STM0_BASE + 0x030u)
#define STM0_CMCON	(BOARD_TIMER_STM0_BASE + 0x038u)
#define STM0_ICR	(BOARD_TIMER_STM0_BASE + 0x03Cu)
#define STM0_ISCR	(BOARD_TIMER_STM0_BASE + 0x040u)

static ulmk_ep_t              g_ep __attribute__((section(".user_bss")));
static volatile uint32_t     *g_stm0 __attribute__((section(".user_bss")));
static ulmk_notif_t           g_irq_notif __attribute__((section(".user_bss")));

static inline uint32_t stm0_off(uint32_t reg)
{
	return (reg - BOARD_TIMER_STM0_BASE) / sizeof(uint32_t);
}

/*
 * Program CMP0 for a one-shot match.  Returns the absolute compare value.
 *
 * QEMU occasionally warps STM0 across the programmed deadline without raising
 * SRC (or drops the edge if ICR was toggled mid-match).  Callers must treat a
 * TIM0-past-CMP0 condition as expiry even if the notification never arrives.
 */
static uint32_t stm0_arm_compare(uint32_t delta_ticks)
{
	uint32_t now;
	uint32_t cmp;

	if (delta_ticks == 0u)
		delta_ticks = 1u;

	/* Disarm + clear sticky match before writing a new deadline. */
	g_stm0[stm0_off(STM0_ICR)]  = 0u;
	g_stm0[stm0_off(STM0_ISCR)] = 0x00000001u;

	now = g_stm0[stm0_off(STM0_TIM0)];
	cmp = now + delta_ticks;
	g_stm0[stm0_off(STM0_CMP0)] = cmp;
	g_stm0[stm0_off(STM0_ISCR)] = 0x00000001u;
	g_stm0[stm0_off(STM0_ICR)]  = 0x00000001u;

	return cmp;
}

static int stm0_expired(uint32_t cmp)
{
	return (int32_t)(g_stm0[stm0_off(STM0_TIM0)] - cmp) >= 0;
}

static uint32_t us_to_ticks(uint32_t us)
{
	uint64_t ticks;

	ticks = ((uint64_t)us * (uint64_t)BOARD_TIMER_HW_CLOCK_HZ) / 1000000u;
	if (ticks == 0u)
		ticks = 1u;
	if (ticks > 0xFFFFFFFFu)
		ticks = 0xFFFFFFFFu;
	return (uint32_t)ticks;
}

static void timer_server(void *arg)
{
	ulmk_msg_t  msg;
	ulmk_msg_t  reply;
	ulmk_tid_t  sender;
	uint32_t    cmp;
	uint32_t    mask;

	(void)arg;

	reply.label    = 0u;
	reply.words[0] = 0u;
	mask = 1u << BOARD_TIMER_NOTIF_BIT;

	for (;;) {
		ulmk_ep_recv(g_ep, &msg, &sender);
		if (msg.label != BOARD_TIMER_MSG_SLEEP) {
			ulmk_ep_reply(sender, &reply);
			continue;
		}

		cmp = stm0_arm_compare(us_to_ticks(msg.words[0]));

		/*
		 * Do not block solely on the IRQ notification: a missed STM0
		 * edge leaves notif_wait hung until 32-bit wrap (~85 s), which
		 * trips the CI 120 s timeout.  Poll + deadline check recovers
		 * when TIM0 advances without SRC, while still honouring a
		 * normal compare-match notif when QEMU delivers it.
		 */
		for (;;) {
			if (ulmk_notif_poll(g_irq_notif, mask) != 0u)
				break;
			if (stm0_expired(cmp))
				break;
			ulmk_thread_yield();
		}

		g_stm0[stm0_off(STM0_ISCR)] = 0x00000001u;
		ulmk_irq_ack(BOARD_TIMER_SRPN);

		ulmk_ep_reply(sender, &reply);
	}
}

void board_timer_sleep_us(uint32_t us)
{
	ulmk_msg_t msg;

	msg.label    = BOARD_TIMER_MSG_SLEEP;
	msg.words[0] = us;
	ulmk_ep_call(g_ep, &msg);
}

uint32_t board_timer_now_ticks(void)
{
	if (!g_stm0)
		return 0u;
	return g_stm0[stm0_off(STM0_TIM0)];
}

uint32_t board_timer_ticks_to_ns(uint32_t dt)
{
	uint64_t ns;

	ns = ((uint64_t)dt * 1000000000ull) / (uint64_t)BOARD_TIMER_HW_CLOCK_HZ;
	if (ns > 0xFFFFFFFFu)
		return 0xFFFFFFFFu;
	return (uint32_t)ns;
}

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;
	int                ret;

	(void)info;

	g_ep = ulmk_ep_create();

	g_stm0 = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_TIMER_STM0_BASE,
		BOARD_TIMER_STM0_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	if (!g_stm0)
		return ULMK_TID_INVALID;

	g_irq_notif = ulmk_notif_create();
	if (g_irq_notif == ULMK_NOTIF_INVALID)
		return ULMK_TID_INVALID;

	ret = ulmk_irq_bind_hw(BOARD_TIMER_SRPN, g_irq_notif,
			       BOARD_TIMER_NOTIF_BIT,
			       (uintptr_t)BOARD_TIMER_SRC_STM0_SR0);
	if (ret != ULMK_OK)
		return ULMK_TID_INVALID;

	ret = ulmk_irq_enable(BOARD_TIMER_SRPN);
	if (ret != ULMK_OK)
		return ULMK_TID_INVALID;

	g_stm0[stm0_off(STM0_CMCON)] = 0x0000001Fu;

	attr.name       = "btimer";
	attr.entry      = timer_server;
	attr.arg        = NULL;
	attr.priority   = 2u;
	attr.stack_size = 2048u;
	attr.privilege  = ULMK_PRIV_DRIVER;

	tid = ulmk_thread_create(&attr);
	if (tid == ULMK_TID_INVALID)
		return ULMK_TID_INVALID;

	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);
	ulmk_cap_grant(tid, ULMK_CAP_IRQ);
	return tid;
}
