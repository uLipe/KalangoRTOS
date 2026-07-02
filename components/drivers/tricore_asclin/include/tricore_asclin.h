/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024-2026 Felipe Neves
 *
 * tricore_asclin.h — public API for the TC27x ASCLIN UART driver component.
 *
 * Each ASCLIN instance (0..TRICORE_ASCLIN_MAX-1) runs as an independent
 * IPC server thread.  Callers use the functions below; IPC is an
 * implementation detail hidden behind this interface.
 *
 * Pin mux (IOCR) must be configured by the board before calling
 * tricore_asclin_init().
 */
#ifndef TRICORE_ASCLIN_H
#define TRICORE_ASCLIN_H

#include <ulmk/microkernel.h>
#include <stdint.h>
#include <stddef.h>

/* Maximum ASCLIN instances on TC27x */
#define TRICORE_ASCLIN_MAX  4u

/*
 * tricore_asclin_init — configure and start ASCLIN instance n.
 *
 * n:      ASCLIN number (0 .. TRICORE_ASCLIN_MAX-1)
 * tx_pin: TX alternate-input select value for ASCLIN.IOCR.ALTI (board-specific)
 * rx_pin: RX alternate-input select value (passed verbatim to IOCR)
 * baud:   desired baud rate in bps (e.g. 115200)
 * fa_hz:  fast peripheral bus frequency in Hz (typically fCPU / 2)
 *
 * Returns the server thread ID on success, ULMK_TID_INVALID on error.
 * The root thread must grant ULMK_CAP_MAP_PERIPH to the returned tid.
 */
ulmk_tid_t tricore_asclin_init(uint8_t n, uint8_t tx_pin, uint8_t rx_pin,
				uint32_t baud, uint32_t fa_hz);

/*
 * tricore_asclin_tx_byte — send a single byte.
 * Returns 0 on success, negative ULMK error code otherwise.
 * Returns -ULMK_ESRCH if instance n was not initialised.
 */
int tricore_asclin_tx_byte(uint8_t n, uint8_t byte);

/*
 * tricore_asclin_tx_buf — send len bytes from buf.
 * Returns number of bytes sent, or negative ULMK error code.
 */
int tricore_asclin_tx_buf(uint8_t n, const uint8_t *buf, size_t len);

/*
 * tricore_asclin_rx_byte — receive a single byte (blocking with timeout).
 * Returns 0 on success, -ULMK_ETIMEOUT if no data within the driver's
 * internal poll limit, or negative error code.
 */
int tricore_asclin_rx_byte(uint8_t n, uint8_t *out);

/*
 * tricore_asclin_rx_buf — receive up to len bytes into buf.
 * Returns number of bytes received, or negative ULMK error code.
 */
int tricore_asclin_rx_buf(uint8_t n, uint8_t *buf, size_t len);

/*
 * tricore_asclin_set_baud — reconfigure baud rate at runtime.
 * Returns 0 on success or negative ULMK error code.
 */
int tricore_asclin_set_baud(uint8_t n, uint32_t baud);

#endif /* TRICORE_ASCLIN_H */
