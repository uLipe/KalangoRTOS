/* SPDX-License-Identifier: MIT */
/*
 * Board services — boards/qemu_mps2_an500/board_services.c
 *
 * A single driver-privilege IPC server owns the CMSDK APB UART0 (polled TX) and
 * the CMSDK APB timer0 (one-shot sleep via NVIC → notification).  One server
 * with a single ep_recv waiter matches the pattern used on the other QEMU
 * boards and avoids interactions with IRQ preemption.
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <arch_config.h>
#include "board_services.h"
#include "board_console.h"
#include "board_timer.h"
#include "board_internal.h"

#define BOARD_TIMER_SRPN	1u
#define BOARD_TIMER_NOTIF_BIT	0u
#define CONSOLE_MSG_PUTC	1u
#define BOARD_TIMER_MSG_SLEEP	2u

/* CMSDK APB UART register indices (32-bit words). */
#define UART_DATA		0u
#define UART_STATE		1u	/* bit0 = TX buffer full */
#define UART_CTRL		2u	/* bit0 = TX enable      */
#define UART_STATE_TXFULL	(1u << 0)
#define UART_CTRL_TXEN		(1u << 0)

/* CMSDK APB timer register indices (32-bit words). */
#define TMR_CTRL		0u	/* bit0 EN, bit3 IRQEN */
#define TMR_VALUE		1u
#define TMR_RELOAD		2u
#define TMR_INTCLR		3u	/* write 1 clears the interrupt */
#define TMR_CTRL_ENABLE		(1u << 0)
#define TMR_CTRL_IRQEN		(1u << 3)

static ulmk_ep_t          g_ep __attribute__((section(".user_bss")));
static volatile uint32_t *g_uart __attribute__((section(".user_bss")));
static volatile uint32_t *g_tmr __attribute__((section(".user_bss")));
static ulmk_notif_t       g_irq_notif __attribute__((section(".user_bss")));

ulmk_ep_t board_service_ep(void)
{
	return g_ep;
}

static void console_putc_hw(char c)
{
	while (g_uart[UART_STATE] & UART_STATE_TXFULL)
		;
	g_uart[UART_DATA] = (uint32_t)(uint8_t)c;
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

static void timer_arm(uint32_t ticks)
{
	g_tmr[TMR_RELOAD] = ticks;
	g_tmr[TMR_VALUE]  = ticks;
	g_tmr[TMR_CTRL]   = TMR_CTRL_ENABLE | TMR_CTRL_IRQEN;
}

static void timer_stop(void)
{
	g_tmr[TMR_CTRL]   = 0u;
	g_tmr[TMR_INTCLR] = 1u;
}

static void board_server(void *arg)
{
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;
	uint32_t   bits;
	int        ret;

	(void)arg;

	g_uart = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_CONSOLE_UART_BASE,
		BOARD_CONSOLE_UART_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	g_tmr = (volatile uint32_t *)ulmk_mem_map(
		(void *)(uintptr_t)BOARD_TIMER_BASE,
		BOARD_TIMER_MAP_SIZE,
		ULMK_PERM_READ | ULMK_PERM_WRITE,
		ULMK_MMAP_PERIPH);
	if (!g_uart || !g_tmr)
		ulmk_thread_exit();

	g_uart[UART_CTRL] = UART_CTRL_TXEN;

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

		timer_arm(us_to_ticks(msg.words[0]));

		bits = 0u;
		ret  = ulmk_notif_wait(g_irq_notif,
				       1u << BOARD_TIMER_NOTIF_BIT, &bits);
		if (ret == ULMK_OK) {
			timer_stop();
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
			       ULMK_ARCH_NVIC_SRC(BOARD_TIMER_NVIC_IRQ));
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
