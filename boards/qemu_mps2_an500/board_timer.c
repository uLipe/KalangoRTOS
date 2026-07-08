/* SPDX-License-Identifier: MIT */
/*
 * Board timer service — boards/qemu_mps2_an500/board_timer.c
 *
 * CMSDK APB timer0 one-shot sleep over NVIC, mirroring the RTC pattern of the
 * other boards.  Production builds use board_services_init() (which owns the
 * timer alongside the console); board_timer_start() remains for integration
 * tests that exercise the timer in isolation.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <arch_config.h>
#include "board_config.h"
#include "board_timer.h"
#include "board_internal.h"

#define BOARD_TIMER_SRPN	1u
#define BOARD_TIMER_NOTIF_BIT	0u
#define BOARD_TIMER_MSG_SLEEP	2u

#define TMR_CTRL		0u
#define TMR_VALUE		1u
#define TMR_RELOAD		2u
#define TMR_INTCLR		3u
#define TMR_CTRL_ENABLE		(1u << 0)
#define TMR_CTRL_IRQEN		(1u << 3)

static ulmk_ep_t          g_ep __attribute__((section(".user_bss")));
static volatile uint32_t *g_tmr __attribute__((section(".user_bss")));
static ulmk_notif_t       g_irq_notif __attribute__((section(".user_bss")));

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
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;
	uint32_t   bits;
	int        ret;

	(void)arg;

	g_tmr = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_TIMER_BASE,
		BOARD_TIMER_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	if (!g_tmr)
		ulmk_thread_exit();

	reply.label    = 0u;
	reply.words[0] = 0u;

	for (;;) {
		ulmk_ep_recv(g_ep, &msg, &sender);
		if (msg.label != BOARD_TIMER_MSG_SLEEP) {
			ulmk_ep_reply(sender, &reply);
			continue;
		}

		g_tmr[TMR_RELOAD] = us_to_ticks(msg.words[0]);
		g_tmr[TMR_VALUE]  = g_tmr[TMR_RELOAD];
		g_tmr[TMR_CTRL]   = TMR_CTRL_ENABLE | TMR_CTRL_IRQEN;

		bits = 0u;
		ret  = ulmk_notif_wait(g_irq_notif,
				       1u << BOARD_TIMER_NOTIF_BIT, &bits);
		if (ret == ULMK_OK) {
			g_tmr[TMR_CTRL]   = 0u;
			g_tmr[TMR_INTCLR] = 1u;
			ulmk_irq_ack(BOARD_TIMER_SRPN);
		}

		ulmk_ep_reply(sender, &reply);
	}
}

void board_timer_sleep_us(uint32_t us)
{
	ulmk_msg_t msg;
	ulmk_ep_t  ep;

	ep = board_service_ep();
	if (ep == ULMK_EP_INVALID)
		ep = g_ep;

	msg.label    = BOARD_TIMER_MSG_SLEEP;
	msg.words[0] = us;
	ulmk_ep_call(ep, &msg);
}

ulmk_tid_t board_timer_start(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;
	int                ret;

	(void)info;

	g_ep = ulmk_ep_create();

	g_irq_notif = ulmk_notif_create();
	if (g_irq_notif == ULMK_NOTIF_INVALID)
		return ULMK_TID_INVALID;

	ret = ulmk_irq_bind_hw(BOARD_TIMER_SRPN, g_irq_notif,
			       BOARD_TIMER_NOTIF_BIT,
			       ULMK_ARCH_NVIC_SRC(BOARD_TIMER_NVIC_IRQ));
	if (ret != ULMK_OK)
		return ULMK_TID_INVALID;

	ret = ulmk_irq_enable(BOARD_TIMER_SRPN);
	if (ret != ULMK_OK)
		return ULMK_TID_INVALID;

	attr.name       = "btimer";
	attr.entry      = timer_server;
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
