/* SPDX-License-Identifier: MIT */
/*
 * ping_pong — two-thread IPC round-trip demo.
 *
 * The synchronous IPC hand-off is used to serialise board-timer access: at any
 * instant exactly one of the two threads is runnable (the other is blocked in
 * ep_call/ep_recv), so only one client ever drives the single board-timer
 * server.  This substitutes for a mutex, which the OS does not provide yet.
 *
 * One round:
 *   ping sleeps 1 s          (ping owns the timer; pong blocked in ep_recv)
 *   ping -> pong (ep_call)   (ping blocks waiting for the reply)
 *   pong sleeps 1 s          (pong owns the timer; ping blocked in ep_call)
 *   pong -> ping (ep_reply)  (pong loops back to ep_recv and blocks)
 *   ping sleeps 1 s          (ping owns the timer again) then repeats
 *
 * Board-agnostic: console and timer via board service C API (link-time).
 */

#include <stdint.h>
#include <ulmk/microkernel.h>
#include <ulmk/linker.h>
#include <ping_pong.h>

void board_console_putc(char c);
void board_console_puts(const char *s);
void board_timer_sleep_us(uint32_t us);

#define PING_PONG_LABEL		1u
#define PING_PRIO		11u	/* below hello_world (10) — see hello_world.c */
#define PONG_PRIO		8u
#define PING_STACK		1024u
#define PONG_STACK		1024u
#define PING_PERIOD_US		1000000u

static ULMK_PRIVATE ulmk_ep_t g_ep;
static ULMK_PRIVATE int       done;

static void print_uint32(uint32_t v)
{
	char buf[11];
	int  i = (int)sizeof(buf) - 1;

	buf[i] = '\0';
	do {
		buf[--i] = (char)('0' + (int)(v % 10u));
		v /= 10u;
	} while (v && i > 0);
	board_console_puts(&buf[i]);
}

static void pong_entry(void *arg)
{
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;

	(void)arg;

	for (;;) {
		ulmk_ep_recv(g_ep, &msg, &sender);

		/* pong owns the timer here: ping is blocked in ep_call(). */
		board_timer_sleep_us(PING_PERIOD_US);

		reply.label     = PING_PONG_LABEL;
		reply.words[0]  = msg.words[0];
		ulmk_ep_reply(sender, &reply);
	}
}

static void ping_entry(void *arg)
{
	ulmk_msg_t msg;
	uint32_t   seq = 0;

	(void)arg;

	for (;;) {
		/* ping owns the timer here: pong is blocked in ep_recv(). */
		board_timer_sleep_us(PING_PERIOD_US);

		msg.label    = PING_PONG_LABEL;
		msg.words[0] = seq++;
		ulmk_ep_call(g_ep, &msg);	/* hand the timer to pong */

		board_console_puts("ping_pong: round ");
		print_uint32(seq);
		board_console_putc('\n');

		/* ping owns the timer again before repeating the cycle. */
		board_timer_sleep_us(PING_PERIOD_US);
	}
}

ulmk_tid_t ping_pong_init(const ulmk_boot_info_t *info)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_tid_t         tid;

	(void)info;

	if (done)
		return ULMK_TID_INVALID;
	done = 1;

	g_ep = ulmk_ep_create();
	if (g_ep == ULMK_EP_INVALID)
		return ULMK_TID_INVALID;

	attr.name       = "pong";
	attr.entry      = pong_entry;
	attr.arg        = NULL;
	attr.priority   = PONG_PRIO;
	attr.stack_size = PONG_STACK;
	attr.privilege  = ULMK_PRIV_USER;
	tid = ulmk_thread_create(&attr);
	if (tid < 0)
		return tid;

	attr.name       = "ping";
	attr.entry      = ping_entry;
	attr.priority   = PING_PRIO;
	attr.stack_size = PING_STACK;
	tid = ulmk_thread_create(&attr);
	return tid;
}
