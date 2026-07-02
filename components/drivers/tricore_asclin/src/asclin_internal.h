/* SPDX-License-Identifier: MIT */
/*
 * asclin_internal.h — private shared definitions between server.c and client.c.
 * Not part of the public API.
 */
#ifndef ASCLIN_INTERNAL_H
#define ASCLIN_INTERNAL_H

#include <ulmk/microkernel.h>
#include <tricore_asclin.h>

/* IPC message labels */
#define ASCLIN_MSG_TX_BYTE   1u
#define ASCLIN_MSG_RX_BYTE   2u
#define ASCLIN_MSG_SET_BAUD  3u

/*
 * RX reply layout:
 *   words[0] = return code (ULMK_OK or negative error)
 *   words[1] = received byte (valid only when words[0] == ULMK_OK)
 */

/*
 * g_eps — one endpoint per ASCLIN instance; defined in server.c.
 * ULMK_EP_INVALID (0) means the instance has not been initialised.
 */
extern ulmk_ep_t g_eps[TRICORE_ASCLIN_MAX];

#endif /* ASCLIN_INTERNAL_H */
