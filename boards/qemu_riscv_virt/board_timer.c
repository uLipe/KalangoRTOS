/* SPDX-License-Identifier: MIT */
/*
 * Board timer service — boards/qemu_riscv_virt/board_timer.c
 *
 * Goldfish RTC (MMIO) compare-match via PLIC, same pattern as STM0 on TriCore.
 * Production builds use board_services_init(); board_timer_start() remains for
 * integration tests that exercise the timer in isolation.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <arch_config.h>
#include "board_timer.h"
#include "board_internal.h"

#define BOARD_TIMER_SRPN		1u
#define BOARD_TIMER_NOTIF_BIT		0u
#define BOARD_TIMER_MSG_SLEEP		1u

#define RTC_TIME_LOW		0x00u
#define RTC_TIME_HIGH		0x04u
#define RTC_ALARM_LOW		0x08u
#define RTC_ALARM_HIGH		0x0Cu
#define RTC_IRQ_ENABLED		0x10u
#define RTC_CLEAR_INTERRUPT	0x1Cu

static ulmk_ep_t          g_ep __attribute__((section(".user_bss")));
static volatile uint32_t *g_rtc __attribute__((section(".user_bss")));
static ulmk_notif_t       g_irq_notif __attribute__((section(".user_bss")));

static inline uint32_t rtc_off(uint32_t reg)
{
	return reg / sizeof(uint32_t);
}

static uint64_t rtc_read_count(void)
{
	uint32_t hi;
	uint32_t lo;

	do {
		hi = g_rtc[rtc_off(RTC_TIME_HIGH)];
		lo = g_rtc[rtc_off(RTC_TIME_LOW)];
	} while (hi != g_rtc[rtc_off(RTC_TIME_HIGH)]);
	return ((uint64_t)hi << 32) | lo;
}

static void rtc_arm_compare(uint32_t delta_ticks)
{
	uint64_t now;
	uint64_t alarm;

	if (delta_ticks == 0u)
		delta_ticks = 1u;

	now   = rtc_read_count();
	alarm = now + (uint64_t)delta_ticks;
	g_rtc[rtc_off(RTC_IRQ_ENABLED)] = 1u;
	g_rtc[rtc_off(RTC_ALARM_HIGH)]  = (uint32_t)(alarm >> 32);
	g_rtc[rtc_off(RTC_ALARM_LOW)]   = (uint32_t)alarm;
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
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;
	uint32_t   bits;
	int        ret;

	(void)arg;

	g_rtc = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_TIMER_RTC_BASE,
		BOARD_TIMER_RTC_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	if (!g_rtc)
		ulmk_thread_exit();

	reply.label    = 0u;
	reply.words[0] = 0u;

	for (;;) {
		ulmk_ep_recv(g_ep, &msg, &sender);
		if (msg.label != BOARD_TIMER_MSG_SLEEP) {
			ulmk_ep_reply(sender, &reply);
			continue;
		}

		rtc_arm_compare(us_to_ticks(msg.words[0]));

		bits = 0u;
		ret  = ulmk_notif_wait(g_irq_notif,
				       1u << BOARD_TIMER_NOTIF_BIT, &bits);
		if (ret == ULMK_OK) {
			g_rtc[rtc_off(RTC_CLEAR_INTERRUPT)] = 1u;
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
			       ULMK_ARCH_PLIC_SRC(BOARD_TIMER_PLIC_IRQ));
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
	if ((int32_t)tid < 0)
		return ULMK_TID_INVALID;

	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);
	ulmk_cap_grant(tid, ULMK_CAP_IRQ);
	return tid;
}
