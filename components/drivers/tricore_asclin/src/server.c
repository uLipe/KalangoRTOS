/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * server.c — TC27x ASCLIN hardware initialisation and IPC server thread.
 *
 * Each ASCLIN instance runs as an independent ULMK_PRIV_DRIVER thread.
 * Hardware access is confined to this file; callers use the IPC wrappers
 * in client.c.
 *
 * The root thread must grant ULMK_CAP_MAP_PERIPH to the tid returned by
 * tricore_asclin_init() before the first IPC call is made.
 */
#include <ulmk/microkernel.h>
#include <stdint.h>
#include "asclin_regs.h"
#include "asclin_internal.h"

/* Server thread stack; fits comfortably inside the default user pool. */
#define ASCLIN_STACK_SIZE    2048u

/*
 * Polling iteration limit before giving up on a TX/RX operation.
 * At ~200 MHz and a tight loop this is approximately 2.5 ms.
 * Integrate with the timer server for production-grade timeouts.
 */
#define ASCLIN_POLL_LIMIT    500000u

/* One register block per instance */
#define ASCLIN_PERIPH_SIZE   0x100u

/* Per-instance data passed as the thread arg */
typedef struct {
	uint8_t   n;
	uint8_t   tx_pin;
	uint8_t   rx_pin;
	uint32_t  baud;
	uint32_t  fa_hz;
	ulmk_ep_t ep;
} asclin_thr_args_t;

/* Global endpoint table — indexed by ASCLIN instance number.
 * Zero-initialised → ULMK_EP_INVALID until tricore_asclin_init() runs. */
ulmk_ep_t g_eps[TRICORE_ASCLIN_MAX];

static asclin_thr_args_t g_args[TRICORE_ASCLIN_MAX];

/* --------------------------------------------------------------------- */

static uint32_t tx_fill(void *base)
{
	return (ASCLIN_REG(base, ASCLIN_TXFIFOCON_OFF) & ASCLIN_TXFIFOCON_FILL_MASK)
		>> ASCLIN_TXFIFOCON_FILL_SHIFT;
}

static uint32_t rx_fill(void *base)
{
	return (ASCLIN_REG(base, ASCLIN_RXFIFOCON_OFF) & ASCLIN_RXFIFOCON_FILL_MASK)
		>> ASCLIN_RXFIFOCON_FILL_SHIFT;
}

/*
 * asclin_hw_set_baud — program BRG for the requested baud rate.
 *
 * Formula (PRESCALER=1, OVERSAMPLING=16):
 *   baud = fA × N / (D × 16)
 *   N    = baud × D × 16 / fA   (rounded to nearest integer)
 *
 * D = 3072 gives sub-1% error for common baud rates at fA ≥ 50 MHz.
 * Adjust D or PRESCALER for low fA or non-standard rates.
 */
static void asclin_hw_set_baud(void *base, uint32_t baud, uint32_t fa_hz)
{
	static const uint32_t DENOM = 3072u;
	uint32_t numer;

	numer = (uint32_t)(((uint64_t)baud * DENOM * 16u) / fa_hz);
	if (numer == 0u) {
		numer = 1u;
	}
	ASCLIN_REG(base, ASCLIN_BRG_OFF) =
		(numer << ASCLIN_BRG_NUMER_SHIFT) |
		(DENOM << ASCLIN_BRG_DENOM_SHIFT);
}

/*
 * asclin_hw_init — configure ASCLIN for 8N1 UART mode.
 *
 * Sequence follows TC27x RM Vol 2 §34.3 (ASCLIN Initialisation):
 *   1. Enable module clock via CLC.
 *   2. Disconnect input clock (CSR.CLKSEL=0) before touching config regs.
 *   3. Configure BITCON, FRAMECON, DATCON, BRG.
 *   4. Configure FIFOs.
 *   5. Reconnect clock and switch FRAMECON.MODE to ASC.
 */
static void asclin_hw_init(void *base, uint8_t tx_pin, uint8_t rx_pin,
			    uint32_t baud, uint32_t fa_hz)
{
	uint32_t wait;

	/* Enable module: DISR=0, poll DISS */
	ASCLIN_REG(base, ASCLIN_CLC_OFF) = 0u;
	for (wait = 0u; wait < 1000u; wait++) {
		if (!(ASCLIN_REG(base, ASCLIN_CLC_OFF) & ASCLIN_CLC_DISS)) {
			break;
		}
	}

	/* Disconnect clock for safe reconfiguration */
	ASCLIN_REG(base, ASCLIN_CSR_OFF) = ASCLIN_CSR_CLKSEL_NOCLK;

	/*
	 * Alternate input select: IOCR.ALTI bits[2:0] selects RXD pin;
	 * bits[6:4] selects TXD output (board-specific alternate function).
	 */
	ASCLIN_REG(base, ASCLIN_IOCR_OFF) =
		((uint32_t)rx_pin & 0x7u) |
		(((uint32_t)tx_pin & 0x7u) << 4);

	/* Bit timing: 16× oversampling, sample at 9/16, prescaler=1 */
	ASCLIN_REG(base, ASCLIN_BITCON_OFF) =
		((uint32_t)ASCLIN_BITCON_OS_16       << ASCLIN_BITCON_OS_SHIFT)  |
		((uint32_t)ASCLIN_BITCON_SP_DEFAULT  << ASCLIN_BITCON_SP_SHIFT)  |
		((uint32_t)ASCLIN_BITCON_PRESC_1     << ASCLIN_BITCON_PRESC_SHIFT);

	/* Frame: 8N1, LSB first, init mode (mode updated at end) */
	ASCLIN_REG(base, ASCLIN_FRAMECON_OFF) =
		ASCLIN_FRAMECON_STOP_1BIT |
		ASCLIN_FRAMECON_PAR_NONE  |
		ASCLIN_FRAMECON_LSB_FIRST |
		ASCLIN_FRAMECON_MODE_INIT;

	ASCLIN_REG(base, ASCLIN_DATCON_OFF) = ASCLIN_DATCON_8BIT;

	asclin_hw_set_baud(base, baud, fa_hz);

	/* Flush then enable TX FIFO output */
	ASCLIN_REG(base, ASCLIN_TXFIFOCON_OFF) = ASCLIN_TXFIFOCON_FLUSH;
	ASCLIN_REG(base, ASCLIN_TXFIFOCON_OFF) = ASCLIN_TXFIFOCON_ENO;

	/* Flush then enable RX FIFO input */
	ASCLIN_REG(base, ASCLIN_RXFIFOCON_OFF) = ASCLIN_RXFIFOCON_FLUSH;
	ASCLIN_REG(base, ASCLIN_RXFIFOCON_OFF) = ASCLIN_RXFIFOCON_ENI;

	/* Reconnect fA clock */
	ASCLIN_REG(base, ASCLIN_CSR_OFF) = ASCLIN_CSR_CLKSEL_FA;

	/* Switch to ASC/UART mode */
	ASCLIN_REG(base, ASCLIN_FRAMECON_OFF) =
		(ASCLIN_REG(base, ASCLIN_FRAMECON_OFF) & ~(7u << 17)) |
		ASCLIN_FRAMECON_MODE_ASC;
}

static int asclin_hw_tx(void *base, uint8_t byte)
{
	uint32_t cnt;

	for (cnt = 0u; cnt < ASCLIN_POLL_LIMIT; cnt++) {
		if (tx_fill(base) < ASCLIN_FIFO_DEPTH) {
			ASCLIN_REG(base, ASCLIN_TXDATA_OFF) = (uint32_t)byte;
			return ULMK_OK;
		}
	}
	return ULMK_ETIMEOUT;
}

static int asclin_hw_rx(void *base, uint8_t *out)
{
	uint32_t cnt;

	for (cnt = 0u; cnt < ASCLIN_POLL_LIMIT; cnt++) {
		if (rx_fill(base) > 0u) {
			*out = (uint8_t)(ASCLIN_REG(base, ASCLIN_RXDATA_OFF) & 0xFFu);
			return ULMK_OK;
		}
	}
	return ULMK_ETIMEOUT;
}

static void asclin_server_entry(void *arg)
{
	asclin_thr_args_t *a = (asclin_thr_args_t *)arg;
	void *base;
	ulmk_msg_t msg;
	ulmk_msg_t reply;
	ulmk_tid_t sender;
	uint8_t b;

	base = ulmk_mem_map((void *)asclin_bases[a->n], ASCLIN_PERIPH_SIZE,
			    ULMK_PERM_READ | ULMK_PERM_WRITE, ULMK_MMAP_PERIPH);

	asclin_hw_init(base, a->tx_pin, a->rx_pin, a->baud, a->fa_hz);

	for (;;) {
		if (ulmk_ep_recv(a->ep, &msg, &sender) != ULMK_OK) {
			continue;
		}

		b = 0u;
		reply.label    = 0u;
		reply.words[0] = (uint32_t)ULMK_EINVAL;
		reply.words[1] = 0u;

		switch (msg.label) {
		case ASCLIN_MSG_TX_BYTE:
			reply.words[0] = (uint32_t)asclin_hw_tx(base,
						(uint8_t)msg.words[0]);
			break;
		case ASCLIN_MSG_RX_BYTE:
			reply.words[0] = (uint32_t)asclin_hw_rx(base, &b);
			reply.words[1] = (uint32_t)b;
			break;
		case ASCLIN_MSG_SET_BAUD:
			asclin_hw_set_baud(base, msg.words[0], a->fa_hz);
			reply.words[0] = (uint32_t)ULMK_OK;
			break;
		default:
			break;
		}

		ulmk_ep_reply(sender, &reply);
	}
}

ulmk_tid_t tricore_asclin_init(uint8_t n, uint8_t tx_pin, uint8_t rx_pin,
				uint32_t baud, uint32_t fa_hz)
{
	ulmk_thread_attr_t attr = {0};
	ulmk_ep_t ep;
	ulmk_tid_t tid;

	if (n >= TRICORE_ASCLIN_MAX) {
		return ULMK_TID_INVALID;
	}
	if (g_eps[n] != ULMK_EP_INVALID) {
		return ULMK_TID_INVALID;
	}

	ep = ulmk_ep_create();
	if (ep == ULMK_EP_INVALID) {
		return ULMK_TID_INVALID;
	}

	g_args[n].n      = n;
	g_args[n].tx_pin = tx_pin;
	g_args[n].rx_pin = rx_pin;
	g_args[n].baud   = baud;
	g_args[n].fa_hz  = fa_hz;
	g_args[n].ep     = ep;

	attr.name       = "asclin";
	attr.entry      = asclin_server_entry;
	attr.arg        = (void *)&g_args[n];
	attr.priority   = 8u;
	attr.stack_size = ASCLIN_STACK_SIZE;
	attr.privilege  = ULMK_PRIV_DRIVER;

	tid = ulmk_thread_create(&attr);
	if (tid == ULMK_TID_INVALID) {
		ulmk_ep_destroy(ep);
		return ULMK_TID_INVALID;
	}

	g_eps[n] = ep;
	return tid;
}
