/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * ASCLIN UART userspace driver — integration test
 *
 * Demonstrates:
 *   - Mapping hardware UART (ASCLIN0) from userspace via ul_mem_map
 *   - Direct MMIO register access (TXDATA, FLAGS, FLAGSENABLE)
 *   - ul_irq_bind for UART TX/RX SRPNs (capability enforcement)
 *   - IPC-based driver API: clients send strings, driver transmits and replies
 *
 * Hardware:
 *   ASCLIN0 base  0xF0000600  size 0x100
 *   SRC base      0xF0038000  size 0x1000
 *   TX SRC index  0x14  (EFS-OpenSource tc27xd_soc.c)
 *   RX SRC index  0x15
 *
 * TX completion detection:
 *   The driver first checks ul_notif_poll for the TX IRQ notification
 *   (works on real TC27x hardware where ASCLIN IRQ is connected to IR).
 *   If the notification is absent it falls back to polling FLAGS.TFL
 *   directly (needed in QEMU dev container where the ASCLIN->IR
 *   connection is not yet wired in the emulated tc27xd_soc).
 *
 * In QEMU -nographic mode ASCLIN0 is connected to serial_hd(0) = stdio,
 * so bytes written via TXDATA appear on QEMU stdout alongside printk.
 */

#include <stdint.h>
#include <stddef.h>
#include <ul/microkernel.h>
#include <kernel/include/ul_printk.h>

/* Provided by boards/qemu_tc27x/qemu_console.c */
extern void qemu_virt_exit(uint32_t code);

/* -------------------------------------------------------------------------
 * ASCLIN0 hardware constants
 * ----------------------------------------------------------------------- */

#define ASCLIN0_BASE		0xF0000600UL
#define ASCLIN0_SIZE		0x100U

#define SRC_BASE		0xF0038000UL
#define SRC_SIZE		0x1000U

/* Register word-indices (byte_offset >> 2) */
#define ASCLIN_CLC		0U
#define ASCLIN_TXFIFOCON	3U
#define ASCLIN_RXFIFOCON	4U
#define ASCLIN_FLAGS		13U
#define ASCLIN_FLAGSCLEAR	15U
#define ASCLIN_FLAGSENABLE	16U
#define ASCLIN_TXDATA		17U
#define ASCLIN_RXDATA		18U
#define ASCLIN_CSR		19U

/* FLAGS register bits */
#define FLAGS_TFL	(1u << 31)	/* TX FIFO level — set after TX */
#define FLAGS_TC	(1u << 17)	/* TX complete */
#define FLAGS_RFL	(1u << 28)	/* RX FIFO level */
#define FLAGS_RFO	(1u << 26)	/* RX FIFO overflow */

/* RXFIFOCON.ENI: accept chardev RX input */
#define RXFIFOCON_ENI	(1u << 1)

/* SRC indices (EFS-OpenSource tc27xd_soc.c) */
#define SRC_IDX_ASCLIN0_TX	0x14U
#define SRC_IDX_ASCLIN0_RX	0x15U

/* SRC register value: SRPN | SRE(bit10) | TOS=0(CPU0) */
#define SRC_VAL(srpn)		((srpn) | (1u << 10))

/* TX poll timeout before giving up and moving on */
#define TX_POLL_LOOPS		50000U

/* -------------------------------------------------------------------------
 * IPC protocol
 * ----------------------------------------------------------------------- */

#define MSG_TX		1U	/* words[0] = pointer to null-terminated string */
#define MSG_TX_OK	2U	/* reply: words[0] = total bytes sent */

/* Notification bit for TX completion IRQ */
#define BIT_TX_DONE	(1u << 0)

#define NUM_CLIENTS	2U

/* -------------------------------------------------------------------------
 * Shared kernel objects
 * ----------------------------------------------------------------------- */

static ul_ep_t    g_uart_ep;
static ul_notif_t g_tx_notif;

/* -------------------------------------------------------------------------
 * Driver private state
 * ----------------------------------------------------------------------- */

static volatile uint32_t *g_asclin;
static volatile uint32_t *g_src;
static uint32_t            g_tx_total;
static uint32_t            g_clients_done;
static uint32_t            g_tx_irq_mode;	/* 1 = IRQ-driven, 0 = poll */

/* -------------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

static inline void asclin_wr(uint32_t idx, uint32_t val)
{
	g_asclin[idx] = val;
}

static inline uint32_t asclin_rd(uint32_t idx)
{
	return g_asclin[idx];
}

/*
 * Transmit one byte.  On real TC27x (IRQ connected): waits on TX notif.
 * On QEMU (IRQ not wired): polls FLAGS.TFL directly.
 */
static void uart_tx_byte(uint8_t byte)
{
	uint32_t bits;
	uint32_t n;

	asclin_wr(ASCLIN_FLAGSCLEAR, FLAGS_TFL | FLAGS_TC);
	asclin_wr(ASCLIN_TXDATA, (uint32_t)byte);

	if (g_tx_irq_mode) {
		ul_notif_wait(g_tx_notif, BIT_TX_DONE, &bits);
	} else {
		/* Poll FLAGS.TFL with a finite timeout */
		n = TX_POLL_LOOPS;
		while (!(asclin_rd(ASCLIN_FLAGS) & FLAGS_TFL) && n--)
			;
	}

	g_tx_total++;
}

static void uart_tx_str(const char *s)
{
	while (*s)
		uart_tx_byte((uint8_t)*s++);
}

/* -------------------------------------------------------------------------
 * UART driver entry
 * ----------------------------------------------------------------------- */

static void uart_driver_entry(void *arg)
{
	void *va_asclin;
	void *va_src;

	(void)arg;

	ul_printk("asclin_integ: driver start\n");

	va_asclin = ul_mem_map((void *)ASCLIN0_BASE, ASCLIN0_SIZE,
			       UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH);
	if (va_asclin != (void *)ASCLIN0_BASE) {
		ul_printk("asclin_integ: FAIL map ASCLIN0\n");
		ul_thread_exit();
	}

	va_src = ul_mem_map((void *)SRC_BASE, SRC_SIZE,
			    UL_PERM_READ | UL_PERM_WRITE, UL_MMAP_PERIPH);
	if (va_src != (void *)SRC_BASE) {
		ul_printk("asclin_integ: FAIL map SRC\n");
		ul_thread_exit();
	}

	g_asclin = (volatile uint32_t *)va_asclin;
	g_src    = (volatile uint32_t *)va_src;

	/* Initialise ASCLIN */
	asclin_wr(ASCLIN_CLC, 0);
	asclin_wr(ASCLIN_CSR, 1);
	asclin_wr(ASCLIN_RXFIFOCON, RXFIFOCON_ENI);
	asclin_wr(ASCLIN_FLAGSENABLE, FLAGS_TFL);

	/* Configure ASCLIN0 TX SRC */
	g_src[SRC_IDX_ASCLIN0_TX] = SRC_VAL(SRC_IDX_ASCLIN0_TX);

	/*
	 * Bind TX SRPN to our notification.
	 * On hardware: when ASCLIN generates a TX IRQ the kernel signals
	 * g_tx_notif/BIT_TX_DONE and uart_tx_byte uses ul_notif_wait.
	 * On QEMU (ASCLIN->IR not wired): binding still validates capability
	 * enforcement; we fall back to FLAGS.TFL polling.
	 */
	if (ul_irq_bind((uint8_t)SRC_IDX_ASCLIN0_TX,
			g_tx_notif, BIT_TX_DONE) != UL_OK) {
		ul_printk("asclin_integ: FAIL irq_bind TX\n");
		ul_thread_exit();
	}

	/*
	 * Probe: attempt one TX with a short poll.  If FLAGS.TFL is set
	 * immediately we are in QEMU polling mode; otherwise IRQ mode.
	 */
	asclin_wr(ASCLIN_FLAGSCLEAR, FLAGS_TFL | FLAGS_TC);
	asclin_wr(ASCLIN_TXDATA, (uint32_t)'!');

	{
		uint32_t probe = ul_notif_poll(g_tx_notif, BIT_TX_DONE);

		if (probe & BIT_TX_DONE) {
			g_tx_irq_mode = 1;
			ul_printk("asclin_integ: TX mode=irq\n");
		} else {
			g_tx_irq_mode = 0;
			ul_printk("asclin_integ: TX mode=poll\n");
		}
		g_tx_total++;	/* count the probe byte */
	}

	ul_printk("asclin_integ: driver ready\n");

	/* Transmit banner; bytes appear on QEMU stdout via serial_hd(0) */
	uart_tx_str("[ASCLIN] ulipeMicroKernel UART driver\r\n");
	ul_printk("asclin_integ: banner sent (%u bytes)\n",
		  (unsigned)g_tx_total);

	/* Serve IPC requests */
	for (;;) {
		ul_msg_t msg;
		ul_tid_t sender;
		ul_msg_t reply;

		ul_ep_recv(g_uart_ep, &msg, &sender);

		if (msg.label == MSG_TX) {
			const char *str =
				(const char *)(uintptr_t)msg.words[0];

			uart_tx_str(str);
			g_clients_done++;

			ul_printk("asclin_integ: client%u done (total_tx=%u)\n",
				  (unsigned)(g_clients_done - 1),
				  (unsigned)g_tx_total);

			reply.label    = MSG_TX_OK;
			reply.words[0] = g_tx_total;
			ul_ep_reply(sender, &reply);

			if (g_clients_done >= NUM_CLIENTS) {
				ul_printk("asclin_integ: PASS\n");
				qemu_virt_exit(0);
				ul_thread_exit();
			}
		}
	}
}

/* -------------------------------------------------------------------------
 * Client threads
 * ----------------------------------------------------------------------- */

static const char g_client0_str[] = "[ASCLIN] hello from client 0\r\n";
static const char g_client1_str[] = "[ASCLIN] hello from client 1\r\n";

static void client_entry(void *arg)
{
	uintptr_t id  = (uintptr_t)arg;
	const char *s = (id == 0) ? g_client0_str : g_client1_str;
	ul_msg_t msg;

	ul_msleep(50);

	msg.label    = MSG_TX;
	msg.words[0] = (uint32_t)(uintptr_t)s;

	ul_ep_call(g_uart_ep, &msg);

	ul_printk("asclin_integ: client%u done (total_tx=%u)\n",
		  (unsigned)id, (unsigned)msg.words[0]);

	ul_thread_exit();
}

/* -------------------------------------------------------------------------
 * Root thread
 * ----------------------------------------------------------------------- */

void ul_root_thread(const ul_boot_info_t *info)
{
	ul_thread_attr_t attr;
	ul_tid_t         driver_tid;

	(void)info;

	ul_printk("asclin_integ: start\n");

	g_uart_ep  = ul_ep_create();
	g_tx_notif = ul_notif_create();

	attr = (ul_thread_attr_t){
		.name       = "uart_drv",
		.entry      = uart_driver_entry,
		.arg        = NULL,
		.priority   = 2,
		.stack_size = 2048,
		.privilege  = UL_PRIV_DRIVER,
	};
	driver_tid = ul_thread_create(&attr);
	ul_cap_grant(driver_tid, UL_CAP_IRQ | UL_CAP_MAP_PERIPH);

	attr = (ul_thread_attr_t){
		.name       = "client0",
		.entry      = client_entry,
		.arg        = (void *)0,
		.priority   = 4,
		.stack_size = 1024,
		.privilege  = UL_PRIV_USER,
	};
	ul_thread_create(&attr);

	attr.name = "client1";
	attr.arg  = (void *)1;
	ul_thread_create(&attr);

	ul_thread_exit();
}
