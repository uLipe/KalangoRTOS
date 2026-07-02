/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * client.c — public API wrappers for the tricore_asclin component.
 *
 * All functions forward to the server thread via IPC.  The server lives
 * in server.c; this file only knows about the endpoint table (g_eps[]).
 */
#include <ulmk/microkernel.h>
#include <stdint.h>
#include <stddef.h>
#include "asclin_internal.h"

static int ep_call_checked(uint8_t n, ulmk_msg_t *msg)
{
	if (n >= TRICORE_ASCLIN_MAX || g_eps[n] == ULMK_EP_INVALID) {
		return ULMK_ESRCH;
	}
	return ulmk_ep_call(g_eps[n], msg);
}

int tricore_asclin_tx_byte(uint8_t n, uint8_t byte)
{
	ulmk_msg_t msg;
	int rc;

	msg.label    = ASCLIN_MSG_TX_BYTE;
	msg.words[0] = (uint32_t)byte;

	rc = ep_call_checked(n, &msg);
	if (rc != ULMK_OK) {
		return rc;
	}
	return (int)(int32_t)msg.words[0];
}

int tricore_asclin_tx_buf(uint8_t n, const uint8_t *buf, size_t len)
{
	size_t i;
	int rc;

	for (i = 0u; i < len; i++) {
		rc = tricore_asclin_tx_byte(n, buf[i]);
		if (rc != ULMK_OK) {
			return (i == 0u) ? rc : (int)i;
		}
	}
	return (int)len;
}

int tricore_asclin_rx_byte(uint8_t n, uint8_t *out)
{
	ulmk_msg_t msg;
	int rc;

	msg.label    = ASCLIN_MSG_RX_BYTE;
	msg.words[0] = 0u;

	rc = ep_call_checked(n, &msg);
	if (rc != ULMK_OK) {
		return rc;
	}
	rc = (int)(int32_t)msg.words[0];
	if (rc == ULMK_OK && out != ((void *)0)) {
		*out = (uint8_t)msg.words[1];
	}
	return rc;
}

int tricore_asclin_rx_buf(uint8_t n, uint8_t *buf, size_t len)
{
	size_t i;
	int rc;

	for (i = 0u; i < len; i++) {
		rc = tricore_asclin_rx_byte(n, &buf[i]);
		if (rc != ULMK_OK) {
			return (i == 0u) ? rc : (int)i;
		}
	}
	return (int)len;
}

int tricore_asclin_set_baud(uint8_t n, uint32_t baud)
{
	ulmk_msg_t msg;
	int rc;

	msg.label    = ASCLIN_MSG_SET_BAUD;
	msg.words[0] = baud;

	rc = ep_call_checked(n, &msg);
	if (rc != ULMK_OK) {
		return rc;
	}
	return (int)(int32_t)msg.words[0];
}
