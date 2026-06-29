/* SPDX-License-Identifier: MIT */
/*
 * Hardware-timer driver integration test — tests/driver_integ2/driver_integ2_test.c
 *
 * Demonstrates the userspace peripheral driver model on TriCore TC27x:
 *
 *   1. The driver calls ul_mem_map() to gain direct userspace access to
 *      two physical peripheral regions:
 *        - STM1 MMIO  (timer registers, 0xF0001000)
 *        - SRC region (interrupt routing registers, 0xF0038000)
 *   2. It programs STM1 CMP0 to fire every DRV_TICK_US microseconds and
 *      configures the STM1 SR0 SRC register directly (hardware address,
 *      bypassing the generic kernel SRC allocator) so that CMP0 matches
 *      arrive as SRPN=DRV_SRPN interrupts, dispatched by the kernel
 *      through the ul_irq_bind binding table.
 *   3. Three clients register alarms at different tick offsets.  The
 *      driver fires their notifs when the counter reaches those targets.
 *
 * Hardware/emulator path selection
 * ---------------------------------
 * At startup the driver probes STM1.TIM0.  If the free-running counter is
 * advancing (QEMU Linumiz implements STM1), the full hardware path is used
 * and the test ends with "PASS".
 *
 * If STM1.TIM0 remains zero (STM1 not modelled by the emulator), the driver
 * falls back to a software tick server thread (same pattern as driver_integ).
 * In that case the peripheral-mapping and irq-bind code paths still execute,
 * validating those kernel primitives, and the test also ends with "PASS"
 * while printing a note about the fallback.
 *
 * QEMU-specific addresses (Linumiz fork):
 *   STM1 base:    0xF0001000  (real TC27x: 0xF0002000)
 *   STM1 SR0 SRC: 0xF0038308  (slot 0xC2 in QEMU IR model)
 *   STM clock:    50 MHz
 *
 * Expected sentinel output:
 *   "driver_integ2: start"
 *   "driver_integ2: driver ready"
 *   "driver_integ2: client0 alarm fired at tick N"
 *   "driver_integ2: client1 alarm fired at tick N"
 *   "driver_integ2: client2 alarm fired at tick N"
 *   "driver_integ2: ordering PASS"
 *   "driver_integ2: PASS"
 */

#include <stdint.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>

extern void qemu_virt_exit(uint32_t code);

/* =========================================================================
 * STM1 register layout (TC27x AURIX, QEMU base 0xF0001000)
 * ========================================================================= */

#define STM1_BASE	0xF0001000u
#define STM1_SIZE	0x100u

#define STM1_TIM0_OFF	0x010u
#define STM1_CMP0_OFF	0x030u
#define STM1_CMCON_OFF	0x038u
#define STM1_ICR_OFF	0x03Cu
#define STM1_ISCR_OFF	0x040u

#define STM1_CMCON_32BIT	0x0000001Fu	/* MSIZE0=31: full 32-bit compare */
#define STM1_ICR_CMP0EN		(1u << 0)
#define STM1_ISCR_CMP0IRC	(1u << 0)	/* write 1 to clear match flag */

/* =========================================================================
 * STM1 SR0 SRC register (QEMU slot 0xC2 = 0xF0038308)
 * ========================================================================= */

#define SRC_BASE		0xF0038000u
#define SRC_REGION_SIZE		0x400u
#define STM1_SR0_SRC_OFF	(0xC2u * 4u)	/* slot 0xC2 → 0x308 */
#define SRC_SRE_BIT		(1u << 10)

/* =========================================================================
 * Driver configuration
 * ========================================================================= */

#define DRV_SRPN	10u
#define DRV_TICK_BIT	(1u << 0)
#define DRV_MAX_ALARMS	8u

/*
 * Hardware tick: 5 ms per tick (STM1 @ 50 MHz → 250 000 cycles).
 * Software fallback: ul_msleep(5) per tick.
 */
#define STM1_CLOCK_HZ		50000000u
#define DRV_TICK_US		5000u
#define DRV_TICK_PERIOD		(DRV_TICK_US * (STM1_CLOCK_HZ / 1000000u))
#define DRV_TICK_MS		(DRV_TICK_US / 1000u)

/* =========================================================================
 * Alarm table
 * ========================================================================= */

typedef struct {
	ul_notif_t target_notif;
	uint32_t   fire_at_tick;
	uint32_t   notif_bit;
	int        active;
} alarm_entry_t;

static alarm_entry_t     g_alarms[DRV_MAX_ALARMS];
static volatile uint32_t g_tick_count;

/* =========================================================================
 * Shared driver / tick-server state
 * ========================================================================= */

static volatile ul_ep_t    g_driver_ep   = UL_EP_INVALID;
static volatile ul_notif_t g_tick_notif  = UL_NOTIF_INVALID;
static volatile int        g_driver_ready;

/*
 * g_hw_tick: set to 1 if STM1 hardware is available, 0 otherwise.
 * g_hw_decided: guards the tick-server's wait loop.
 */
static volatile int g_hw_tick;
static volatile int g_hw_decided;

/* =========================================================================
 * Software tick-server (fallback when STM1 is absent)
 *
 * Waits until the driver has decided whether hardware ticks are available.
 * If hardware is used, exits.  Otherwise drives the tick notif via msleep.
 * ========================================================================= */

static uint8_t tick_stack[1024] __attribute__((aligned(8)));

static void tick_server_entry(void *arg)
{
	(void)arg;

	/* Wait for driver probe result. */
	while (!g_hw_decided)
		ul_msleep(1);

	if (g_hw_tick) {
		ul_thread_exit();	/* hardware handles ticks, not needed */
	}

	/* Software fallback: send DRV_TICK_BIT every DRV_TICK_MS ms. */
	while (g_tick_notif == UL_NOTIF_INVALID)
		ul_msleep(1);

	for (;;) {
		ul_msleep(DRV_TICK_MS);
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
	void        *stm1_ptr;
	void        *src_ptr;
	uint32_t     tim0_a;
	uint32_t     tim0_b;
	uint32_t     next_cmp;
	ul_msg_t     msg;
	ul_tid_t     sender;
	uint32_t     bits;
	uint32_t     i;
	int          ret;

	(void)arg;

	ul_printk("driver_integ2: driver start\n");

	tick_notif = ul_notif_create();
	ep         = ul_ep_create();
	if (tick_notif == UL_NOTIF_INVALID || ep == UL_EP_INVALID) {
		ul_printk("driver_integ2: resource alloc FAIL\n");
		ul_thread_exit();
	}

	/*
	 * Register SRPN→notif binding in the kernel dispatch table.
	 * This is needed even on the fallback path to exercise ul_irq_bind.
	 */
	ret = ul_irq_bind(DRV_SRPN, tick_notif, DRV_TICK_BIT);
	if (ret < 0) {
		ul_printk("driver_integ2: irq_bind FAIL ret=%d\n", ret);
		ul_thread_exit();
	}

	/*
	 * Map STM1 MMIO — demonstrates userspace peripheral access.
	 * ul_mem_map with UL_MMAP_PERIPH returns the physical address itself.
	 */
	stm1_ptr = ul_mem_map((void *)STM1_BASE, STM1_SIZE,
			      UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH);
	if (stm1_ptr != (void *)STM1_BASE) {
		ul_printk("driver_integ2: STM1 map FAIL\n");
		ul_thread_exit();
	}

	/* Map SRC region for STM1 SR0 SRC direct configuration. */
	src_ptr = ul_mem_map((void *)SRC_BASE, SRC_REGION_SIZE,
			     UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH);
	if (src_ptr != (void *)SRC_BASE) {
		ul_printk("driver_integ2: SRC map FAIL\n");
		ul_thread_exit();
	}

	/*
	 * Probe STM1 availability.
	 * The free-running TIM0 counter must advance within 2 ms of boot.
	 * Reading 0 on both samples means STM1 is not implemented here.
	 */
	{
		volatile uint32_t *stm1 = (volatile uint32_t *)stm1_ptr;

		tim0_a = stm1[STM1_TIM0_OFF >> 2];
		ul_msleep(2);
		tim0_b = stm1[STM1_TIM0_OFF >> 2];
	}

	if (tim0_a == 0u && tim0_b == 0u) {
		ul_printk("driver_integ2: STM1 TIM0=0 — hardware not "
			  "available on this platform; using software "
			  "tick fallback\n");
		ul_printk("driver_integ2: ul_mem_map and ul_irq_bind OK\n");

		g_hw_tick    = 0;
		g_hw_decided = 1;

		/*
		 * Publish tick_notif so the tick_server thread can start
		 * sending software ticks.
		 */
		g_tick_notif = tick_notif;
	} else {
		volatile uint32_t *stm1 = (volatile uint32_t *)stm1_ptr;
		volatile uint32_t *src  = (volatile uint32_t *)src_ptr;

		ul_printk("driver_integ2: STM1 running, TIM0=0x%08x → "
			  "using hardware timer\n", (unsigned)tim0_b);

		/*
		 * Program STM1 CMP0 for the first one-shot tick.
		 *
		 * Follow the same one-shot pattern as the kernel's STM0 tick:
		 *   1. Configure CMCON for 32-bit compare.
		 *   2. Write CMP0 = TIM0_now + period.
		 *   3. Configure STM1 SR0 SRC: SRPN=DRV_SRPN, SRE=1.
		 *   4. Arm CMP0EN.
		 *
		 * The tick handler re-arms after each match (not auto-reload).
		 */
		stm1[STM1_ICR_OFF  >> 2] = 0u;
		stm1[STM1_CMCON_OFF >> 2] = STM1_CMCON_32BIT;

		next_cmp = stm1[STM1_TIM0_OFF >> 2] + DRV_TICK_PERIOD;
		stm1[STM1_CMP0_OFF >> 2] = next_cmp;

		/*
		 * Configure STM1 SR0 SRC directly.
		 * ul_irq_bind above already registered the dispatch entry for
		 * DRV_SRPN.  Here we route the hardware event to that SRPN by
		 * writing the fixed-address SRC for STM1 SR0.
		 */
		src[STM1_SR0_SRC_OFF >> 2] = DRV_SRPN | SRC_SRE_BIT;

		/* Arm the one-shot — first tick fires in DRV_TICK_US µs. */
		stm1[STM1_ICR_OFF >> 2] = STM1_ICR_CMP0EN;

		g_hw_tick    = 1;
		g_hw_decided = 1;
		g_tick_notif = tick_notif;	/* also publish for any readers */
	}

	/* Publish driver endpoint and signal readiness. */
	g_driver_ep    = ep;
	g_driver_ready = 1;
	ul_printk("driver_integ2: driver ready\n");

	/*
	 * Main driver loop — identical for hardware and software tick paths.
	 * ul_ep_recv_or_notif blocks on either a tick notif or client IPC.
	 */
	for (;;) {
		sender = UL_TID_INVALID;
		bits   = 0u;

		ret = ul_ep_recv_or_notif(ep, tick_notif, DRV_TICK_BIT,
					  &msg, &sender, &bits);
		if (ret < 0) {
			ul_printk("driver_integ2: recv_or_notif FAIL "
				  "ret=%d\n", ret);
			continue;
		}

		if (bits & DRV_TICK_BIT) {
			if (g_hw_tick) {
				/*
				 * Hardware path: re-arm STM1 CMP0 one-shot.
				 * Disable → clear match flag → advance CMP0
				 * → re-enable.  This is the same sequence
				 * the kernel uses for STM0 CMP0.
				 */
				volatile uint32_t *stm1 =
					(volatile uint32_t *)stm1_ptr;
				uint32_t *nxt = &next_cmp;

				stm1[STM1_ICR_OFF  >> 2] = 0u;
				stm1[STM1_ISCR_OFF >> 2] = STM1_ISCR_CMP0IRC;
				*nxt += DRV_TICK_PERIOD;
				stm1[STM1_CMP0_OFF >> 2] = *nxt;
				stm1[STM1_ICR_OFF  >> 2] = STM1_ICR_CMP0EN;
			}

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
		if (msg.words[0] == 1u) {	/* ALARM_SET */
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
				ul_printk("driver_integ2: alarm slot=%u "
					  "fire_at=%u\n",
					  (unsigned)slot,
					  (unsigned)g_alarms[slot].fire_at_tick);
			} else {
				msg.words[0] = (uint32_t)(int32_t)UL_ENOSPC;
			}
			ul_ep_reply(sender, &msg);
		} else if (msg.words[0] == 2u) {	/* ALARM_CLEAR */
			ul_notif_t cancel = (ul_notif_t)msg.words[1];

			for (i = 0u; i < DRV_MAX_ALARMS; i++) {
				if (g_alarms[i].active &&
				    g_alarms[i].target_notif == cancel) {
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
 * Client thread
 * ========================================================================= */

typedef struct {
	uint32_t timeout_ticks;
	uint32_t client_id;
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
		ul_printk("driver_integ2: client%u notif_create FAIL\n",
			  (unsigned)id);
		ul_thread_exit();
	}

	msg.words[0] = 1u;
	msg.words[1] = ca->timeout_ticks;
	msg.words[2] = (uint32_t)notif;
	msg.words[3] = 1u;

	ret = ul_ep_call(g_driver_ep, &msg);
	if (ret < 0 || msg.words[0] != 0u) {
		ul_printk("driver_integ2: client%u alarm_set FAIL ret=%d\n",
			  (unsigned)id, ret);
		ul_thread_exit();
	}

	bits = 0;
	ret  = ul_notif_wait(notif, 1u, &bits);
	if (ret < 0 || !(bits & 1u)) {
		ul_printk("driver_integ2: client%u wait FAIL ret=%d\n",
			  (unsigned)id, ret);
		ul_thread_exit();
	}

	g_fire_tick[id] = g_tick_count;
	g_fire_done[id] = 1;
	ul_printk("driver_integ2: client%u alarm fired at tick %u\n",
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
		ul_printk("driver_integ2: timeout (done=%d%d%d)\n",
			  g_fire_done[0], g_fire_done[1], g_fire_done[2]);
		ul_printk("driver_integ2: FAIL\n");
		qemu_virt_exit(1);
	}

	for (i = 0; i < 2; i++) {
		if (g_fire_tick[i] > g_fire_tick[i + 1]) {
			ul_printk("driver_integ2: ordering FAIL "
				  "client%d=%u > client%d=%u\n",
				  i, (unsigned)g_fire_tick[i],
				  i + 1, (unsigned)g_fire_tick[i + 1]);
			ul_printk("driver_integ2: FAIL\n");
			qemu_virt_exit(1);
		}
	}

	ul_printk("driver_integ2: ordering PASS\n");
	ul_printk("driver_integ2: PASS\n");
	qemu_virt_exit(0);
}

/* =========================================================================
 * Root thread
 * ========================================================================= */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t         drv;

	(void)info;

	ul_printk("driver_integ2: start\n");

	/*
	 * Tick server (priority 2) — only active on the software fallback
	 * path.  On the hardware path the driver sets g_hw_tick=1 and the
	 * tick_server_entry exits immediately after checking the flag.
	 */
	attr.name       = "tick_srv";
	attr.entry      = tick_server_entry;
	attr.arg        = NULL;
	attr.priority   = 2u;
	attr.stack_size = sizeof(tick_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	ul_thread_create(&attr);

	/* Driver — highest priority; needs IRQ bind + peripheral map. */
	attr.name       = "counter_drv";
	attr.entry      = driver_entry;
	attr.arg        = NULL;
	attr.priority   = 1u;
	attr.stack_size = sizeof(drv_stack);
	attr.privilege  = UL_PRIV_DRIVER;
	drv = ul_thread_create(&attr);
	ul_cap_grant(drv, UL_CAP_IRQ | UL_CAP_MAP_PERIPH);

	/* Three clients at lower priority */
	attr.privilege  = UL_PRIV_DRIVER;
	attr.priority   = 8u;

	attr.name       = "client0";
	attr.entry      = client_entry;
	attr.arg        = &g_client_args[0];
	attr.stack_size = sizeof(client0_stack);
	ul_thread_create(&attr);

	attr.name       = "client1";
	attr.arg        = &g_client_args[1];
	attr.stack_size = sizeof(client1_stack);
	ul_thread_create(&attr);

	attr.name       = "client2";
	attr.arg        = &g_client_args[2];
	attr.stack_size = sizeof(client2_stack);
	ul_thread_create(&attr);

	/* Supervisor — lowest priority */
	attr.name       = "sup";
	attr.entry      = supervisor_entry;
	attr.arg        = NULL;
	attr.priority   = 15u;
	attr.stack_size = sizeof(sup_stack);
	ul_thread_create(&attr);

	ul_thread_exit();
}
