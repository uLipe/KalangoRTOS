/* SPDX-License-Identifier: MIT */
/*
 * Driver integration test — tests/driver_integ/driver_integ_test.c
 *
 * Implements a userspace counter-driver that exposes alarm functionality:
 *
 *   - A dedicated "tick server" thread wakes every millisecond via ul_msleep
 *     and signals the driver's tick notif.  This correctly models a hardware
 *     timer interrupt without the scheduling-priority inversion of a
 *     self-triggered SRPN (which fires before ul_ep_recv_or_notif blocks).
 *
 *   - Clients register alarms by sending an IPC message to the driver's
 *     endpoint:
 *       msg.words[0] = ALARM_SET
 *       msg.words[1] = timeout_ticks
 *       msg.words[2] = client notif handle
 *       msg.words[3] = bit to signal
 *     The driver replies immediately, then signals the client's notification
 *     when the alarm fires.
 *
 *   - Three clients register alarms at different intervals and verify they
 *     are notified in the correct order.
 *
 * Sentinel output expected by the Makefile:
 *   "driver_integ: start"
 *   "driver_integ: driver ready"
 *   "driver_integ: client0 alarm fired at tick N"
 *   "driver_integ: client1 alarm fired at tick N"
 *   "driver_integ: client2 alarm fired at tick N"
 *   "driver_integ: ordering PASS"
 *   "driver_integ: PASS"
 */

#include <stdint.h>
#include "../test_support.h"
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>


/* =========================================================================
 * Protocol constants
 * ========================================================================= */

#define ALARM_SET	1u
#define ALARM_CLEAR	2u

#define DRV_TICK_BIT	(1u << 0)
#define DRV_MAX_ALARMS	8u

/* =========================================================================
 * Driver internal state
 * ========================================================================= */

typedef struct {
	ul_notif_t target_notif;	/* notif to signal when alarm fires */
	uint32_t   fire_at_tick;	/* absolute counter tick value      */
	uint32_t   notif_bit;		/* bit to signal                    */
	int        active;
} alarm_entry_t;

static alarm_entry_t g_alarms[DRV_MAX_ALARMS];
static volatile uint32_t g_tick_count;

/* =========================================================================
 * Shared driver state — published by the driver thread
 * ========================================================================= */

static volatile ul_ep_t    g_driver_ep    = UL_EP_INVALID;
static volatile ul_notif_t g_tick_notif   = UL_NOTIF_INVALID;
static volatile int        g_driver_ready;

/* =========================================================================
 * Tick server — separate low-priority thread that drives the counter.
 *
 * Fires every 1 ms via ul_msleep, then signals g_tick_notif.  This keeps
 * the tick asynchronous with respect to the driver's IPC handling: the
 * driver is blocked in ul_ep_recv_or_notif when the signal arrives, so
 * any pending IPC is processed first on the next call.
 * ========================================================================= */

static uint8_t tick_stack[1024] __attribute__((aligned(8)));

static void tick_server_entry(void *arg)
{
	(void)arg;

	/* Wait until the driver publishes the tick notif. */
	while (g_tick_notif == UL_NOTIF_INVALID)
		ul_msleep(1);

	for (;;) {
		ul_msleep(1);
		ul_notif_signal(g_tick_notif, DRV_TICK_BIT);
	}
}

/* =========================================================================
 * Driver thread
 * ========================================================================= */

static uint8_t drv_stack[4096] __attribute__((aligned(8)));

static void driver_entry(void *arg)
{
	ul_notif_t   tick_notif;
	ul_ep_t      ep;
	ul_msg_t     msg;
	ul_tid_t     sender;
	uint32_t     bits;
	uint32_t     i;
	int          ret;

	(void)arg;

	ul_printk("driver_integ: driver start\n");

	tick_notif = ul_notif_create();
	ep         = ul_ep_create();

	if (tick_notif == UL_NOTIF_INVALID || ep == UL_EP_INVALID) {
		ul_printk("driver_integ: driver create FAIL\n");
		ul_thread_exit();
	}

	/* Publish state and signal readiness. */
	g_tick_notif   = tick_notif;
	g_driver_ep    = ep;
	g_driver_ready = 1;
	ul_printk("driver_integ: driver ready\n");

	/*
	 * Main driver loop: multiplex between IPC (ALARM_SET/ALARM_CLEAR from
	 * clients) and tick notifications from the tick server.
	 *
	 * Because the tick server is a separate thread, it only signals
	 * tick_notif while the driver is blocked here — so IPC is never
	 * starved by the tick stream.
	 */
	for (;;) {
		sender = UL_TID_INVALID;
		bits   = 0u;

		ret = ul_ep_recv_or_notif(ep, tick_notif, DRV_TICK_BIT,
					  &msg, &sender, &bits);
		if (ret < 0) {
			ul_printk("driver_integ: recv_or_notif FAIL ret=%d\n",
				  ret);
			continue;
		}

		if (bits & DRV_TICK_BIT) {
			g_tick_count++;

			for (i = 0u; i < DRV_MAX_ALARMS; i++) {
				if (!g_alarms[i].active)
					continue;
				if (g_tick_count >= g_alarms[i].fire_at_tick) {
					ul_notif_signal(g_alarms[i].target_notif,
							g_alarms[i].notif_bit);
					g_alarms[i].active = 0;
				}
			}
			continue;
		}

		/* IPC message from a client */
		if (msg.words[0] == ALARM_SET) {
			uint32_t timeout = msg.words[1];
			uint32_t notif_h = msg.words[2];
			uint32_t notif_b = msg.words[3];
			uint32_t slot    = DRV_MAX_ALARMS;

			for (i = 0u; i < DRV_MAX_ALARMS; i++) {
				if (!g_alarms[i].active) {
					slot = i;
					break;
				}
			}

			if (slot < DRV_MAX_ALARMS) {
				g_alarms[slot].target_notif = (ul_notif_t)notif_h;
				g_alarms[slot].fire_at_tick = g_tick_count +
							      timeout;
				g_alarms[slot].notif_bit    = notif_b;
				g_alarms[slot].active       = 1;
				msg.words[0] = 0u;
				ul_printk("driver_integ: alarm set slot=%u "
					  "fire_at=%u\n",
					  (unsigned)slot,
					  (unsigned)g_alarms[slot].fire_at_tick);
			} else {
				msg.words[0] = (uint32_t)(int32_t)UL_ENOSPC;
			}

			ul_ep_reply(sender, &msg);
		} else if (msg.words[0] == ALARM_CLEAR) {
			ul_notif_t cancel_notif = (ul_notif_t)msg.words[1];

			for (i = 0u; i < DRV_MAX_ALARMS; i++) {
				if (g_alarms[i].active &&
				    g_alarms[i].target_notif == cancel_notif) {
					g_alarms[i].active = 0;
					break;
				}
			}
			msg.words[0] = 0u;
			ul_ep_reply(sender, &msg);
		} else {
			msg.words[0] = (uint32_t)(int32_t)UL_EINVAL;
			ul_ep_reply(sender, &msg);
		}
	}
}

/* =========================================================================
 * Client thread — registers an alarm and waits for it to fire
 * ========================================================================= */

typedef struct {
	uint32_t   timeout_ticks;
	uint32_t   client_id;
} client_arg_t;

static uint8_t client0_stack[2048] __attribute__((aligned(8)));
static uint8_t client1_stack[2048] __attribute__((aligned(8)));
static uint8_t client2_stack[2048] __attribute__((aligned(8)));

static volatile uint32_t g_fire_tick[3];
static volatile int      g_fire_done[3];

static void client_entry(void *arg)
{
	const client_arg_t *ca = (const client_arg_t *)arg;
	ul_notif_t  notif;
	ul_msg_t    msg;
	uint32_t    bits;
	int         ret;
	uint32_t    id = ca->client_id;

	while (!g_driver_ready)
		ul_msleep(5);

	notif = ul_notif_create();
	if (notif == UL_NOTIF_INVALID) {
		ul_printk("driver_integ: client%u notif_create FAIL\n",
			  (unsigned)id);
		ul_thread_exit();
	}

	msg.words[0] = ALARM_SET;
	msg.words[1] = ca->timeout_ticks;
	msg.words[2] = (uint32_t)notif;
	msg.words[3] = 1u;

	ret = ul_ep_call(g_driver_ep, &msg);
	if (ret < 0 || msg.words[0] != 0u) {
		ul_printk("driver_integ: client%u alarm_set FAIL ret=%d\n",
			  (unsigned)id, ret);
		ul_thread_exit();
	}

	bits = 0;
	ret  = ul_notif_wait(notif, 1u, &bits);
	if (ret < 0 || !(bits & 1u)) {
		ul_printk("driver_integ: client%u wait FAIL ret=%d bits=0x%x\n",
			  (unsigned)id, ret, (unsigned)bits);
		ul_thread_exit();
	}

	g_fire_tick[id] = g_tick_count;
	g_fire_done[id] = 1;
	ul_printk("driver_integ: client%u alarm fired at tick %u\n",
		  (unsigned)id, (unsigned)g_fire_tick[id]);

	ul_thread_exit();
}

/* =========================================================================
 * Supervisor
 * ========================================================================= */

static uint8_t sup_stack[2048] __attribute__((aligned(8)));

static client_arg_t g_client_args[3] = {
	{ .timeout_ticks = 5u,  .client_id = 0u },
	{ .timeout_ticks = 10u, .client_id = 1u },
	{ .timeout_ticks = 20u, .client_id = 2u },
};

static void supervisor_entry(void *arg)
{
	uint32_t waited;
	int      i;

	(void)arg;

	waited = 0;
	while (waited < 10000u) {
		if (g_fire_done[0] && g_fire_done[1] && g_fire_done[2])
			break;
		ul_msleep(20);
		waited += 20u;
	}

	if (!g_fire_done[0] || !g_fire_done[1] || !g_fire_done[2]) {
		ul_printk("driver_integ: timeout waiting for alarms "
			  "(done=%d%d%d)\n",
			  g_fire_done[0], g_fire_done[1], g_fire_done[2]);
		ul_printk("driver_integ: FAIL\n");
		ul_sim_exit(1);
	}

	for (i = 0; i < 2; i++) {
		if (g_fire_tick[i] > g_fire_tick[i + 1]) {
			ul_printk("driver_integ: ordering FAIL "
				  "client%d tick=%u > client%d tick=%u\n",
				  i, (unsigned)g_fire_tick[i],
				  i + 1, (unsigned)g_fire_tick[i + 1]);
			ul_printk("driver_integ: FAIL\n");
			ul_sim_exit(1);
		}
	}

	ul_printk("driver_integ: ordering PASS\n");
	ul_printk("driver_integ: PASS\n");
	ul_sim_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;

	(void)info;

	ul_printk("driver_integ: start\n");

	/* Tick server — priority 2, just below driver */
	attr.name       = "tick_srv";
	attr.entry      = tick_server_entry;
	attr.arg        = NULL;
	attr.priority   = 2u;
	attr.stack_size = sizeof(tick_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	/* Driver — highest priority */
	attr.name       = "counter_drv";
	attr.entry      = driver_entry;
	attr.arg        = NULL;
	attr.priority   = 1u;
	attr.stack_size = sizeof(drv_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	/* Three clients at lower priority */
	attr.name       = "client0";
	attr.entry      = client_entry;
	attr.arg        = &g_client_args[0];
	attr.priority   = 8u;
	attr.stack_size = sizeof(client0_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "client1";
	attr.entry      = client_entry;
	attr.arg        = &g_client_args[1];
	attr.priority   = 8u;
	attr.stack_size = sizeof(client1_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	attr.name       = "client2";
	attr.entry      = client_entry;
	attr.arg        = &g_client_args[2];
	attr.priority   = 8u;
	attr.stack_size = sizeof(client2_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	/* Supervisor — lowest priority, just watches */
	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 15u;
	attr.stack_size = sizeof(sup_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	ul_thread_exit();
}
