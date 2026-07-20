/* SPDX-License-Identifier: MIT */
/*
 * Board services — boards/qemu_riscv_virt/board_services.c
 *
 * Single IPC server thread for console and timer.  Two separate ep_recv
 * waiters interact badly with RV32 IRQ preemption (see ulmk_kern_sched_dispatch).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <arch_config.h>
#include "board_services.h"
#include "board_console.h"
#include "board_timer.h"
#include "board_internal.h"

#define BOARD_TIMER_SRPN		1u
#define BOARD_TIMER_NOTIF_BIT		0u
#define BOARD_TIMER_MSG_SLEEP		1u
#define CONSOLE_MSG_PUTC		1u

#define UART_LSR			5u
#define UART_LSR_TX_IDLE		(1u << 5)

#define RTC_TIME_LOW			0x00u
#define RTC_TIME_HIGH			0x04u
#define RTC_ALARM_LOW			0x08u
#define RTC_ALARM_HIGH			0x0Cu
#define RTC_IRQ_ENABLED			0x10u
#define RTC_CLEAR_INTERRUPT		0x1Cu

static ulmk_ep_t          g_ep __attribute__((section(".user_bss")));
static volatile uint32_t *g_rtc __attribute__((section(".user_bss")));
static volatile uint8_t *g_uart __attribute__((section(".user_bss")));
static ulmk_notif_t       g_irq_notif __attribute__((section(".user_bss")));

ulmk_ep_t board_service_ep(void)
{
	return g_ep;
}

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

static void console_putc_hw(char c)
{
	while (!(g_uart[UART_LSR] & UART_LSR_TX_IDLE))
		;
	g_uart[0] = (uint8_t)c;
}

static void board_server(void *arg)
{
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;
	uint32_t   bits;
	int        ret;

	(void)arg;

	g_uart = (volatile uint8_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_CONSOLE_UART_BASE,
		BOARD_CONSOLE_UART_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	g_rtc = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_TIMER_RTC_BASE,
		BOARD_TIMER_RTC_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	if (!g_uart || !g_rtc)
		ulmk_thread_exit();

	reply.label    = 0u;
	reply.words[0] = 0u;

	for (;;) {
		ulmk_ep_recv(g_ep, &msg, &sender);

		if (msg.label == CONSOLE_MSG_PUTC) {
			console_putc_hw((char)(uint8_t)msg.words[0]);
			ulmk_ep_reply(sender, &reply);
			continue;
		}

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

void ulmk_board_init(void)
{
}

void board_services_init(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;
	int                ret;

	(void)info;

	g_ep = ulmk_ep_create();

	g_irq_notif = ulmk_notif_create();
	if (g_irq_notif == ULMK_NOTIF_INVALID)
		return;

	ret = ulmk_irq_bind_hw(BOARD_TIMER_SRPN, g_irq_notif,
			       BOARD_TIMER_NOTIF_BIT,
			       ULMK_ARCH_PLIC_SRC(BOARD_TIMER_PLIC_IRQ));
	if (ret != ULMK_OK)
		return;

	ret = ulmk_irq_enable(BOARD_TIMER_SRPN);
	if (ret != ULMK_OK)
		return;

	attr.name       = "bsvc";
	attr.entry      = board_server;
	attr.priority   = 1u;
	attr.stack_size = 4096u;
	attr.privilege  = ULMK_PRIV_DRIVER;

	tid = ulmk_thread_create(&attr);
	if (tid == ULMK_TID_INVALID)
		return;

	ulmk_cap_grant(tid, ULMK_CAP_MAP_PERIPH);
	ulmk_cap_grant(tid, ULMK_CAP_IRQ);
}
